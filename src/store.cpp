#include "store.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRandomGenerator>
#include <QImageReader>
#include <QImage>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QThread>
#include <QUrl>
#include <QSet>
#include <QRegularExpression>
#include <QHostAddress>
#include <QUuid>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace jp {
QStringList panelCategories(){return {"Logos","Banner","QR-Codes","Vinhetas","Abertura","Vídeos"};}
QString panelCategory(const QString &category,const QString &mime){
    if(panelCategories().contains(category))return category;
    if(category=="Banners"||category=="Tarjetas"||category=="Rodapés")return "Banner";
    if(category=="Overlays"||category=="Animações")return "Vinhetas";
    return mime.startsWith("video/")?"Vídeos":"Banner";
}
QString randomKey() {
    QByteArray b(32, '\0');
    for (int i = 0; i < b.size(); i += 4) {
        const auto v = QRandomGenerator::system()->generate();
        memcpy(b.data() + i, &v, 4);
    }
    return QString::fromLatin1(b.toHex());
}
static QString nameChecked(const QString &n) {
    const auto s = n.trimmed();
    if (s.isEmpty() || s.size() > 120) throw Error("Use um nome de 1 a 120 caracteres.");
    return s;
}
static QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString mediaType(const QByteArray &h) {
    if (h.startsWith(QByteArray::fromHex("89504e470d0a1a0a"))) return "image/png";
    if (h.startsWith(QByteArray::fromHex("ffd8ff"))) return "image/jpeg";
    if (h.startsWith("GIF87a") || h.startsWith("GIF89a")) return "image/gif";
    if (h.startsWith("RIFF") && h.mid(8,4) == "WEBP") return "image/webp";
    if (h.startsWith(QByteArray::fromHex("1a45dfa3")) && h.left(4096).contains("webm")) return "video/webm";
    if (h.size() >= 16 && h.mid(4,4) == "ftyp") {
        const auto brands = h.mid(8, qMin<qsizetype>(256, h.size() - 8));
        if (brands.contains("avif") || brands.contains("avis") || brands.contains("heic")) return {};
        if (brands.contains("isom") || brands.contains("iso2") || brands.contains("mp41") ||
            brands.contains("mp42") || brands.contains("avc1") || brands.contains("M4V ")) return "video/mp4";
    }
    return {};
}
bool publicAddress(const QString &s) {
    QHostAddress a(s);
    if (a.isNull() || a.isLoopback() || a.isMulticast() || a == QHostAddress::Any || a == QHostAddress::AnyIPv6) return false;
    bool v4 = false;
    const auto ip = a.toIPv4Address(&v4);
    if (v4) {
        const auto x = ip >> 24, y = (ip >> 16) & 255;
        if (x == 0 || x == 10 || x == 127 || x >= 224 || (x == 169 && y == 254) ||
            (x == 172 && y >= 16 && y <= 31) || (x == 192 && (y == 168 || y == 0 || y == 2)) ||
            (x == 100 && y >= 64 && y <= 127) || (x == 198 && (y == 18 || y == 19 || y == 51)) ||
            (x == 203 && y == 0)) return false;
        return true;
    }
    // Only global unicast IPv6; exclude documentation, 6to4 and Teredo ranges.
    return a.isInSubnet(QHostAddress("2000::"), 3) && !a.isInSubnet(QHostAddress("2001::"), 23) &&
        !a.isInSubnet(QHostAddress("2001:db8::"), 32) && !a.isInSubnet(QHostAddress("2002::"), 16);
}
Store::Store(QString root, QObject *parent) : QObject(parent), root_(std::move(root)) {}
Store::~Store(){closing_=true;for(const auto &job:importQueue_)if(job.value(4)=="remove")QFile::remove(job.value(1));importQueue_.clear();const auto threads=findChildren<QThread*>(QString(),Qt::FindDirectChildrenOnly);for(auto *thread:threads)thread->requestInterruption();for(auto *thread:threads)thread->wait();}
void Store::open() {
    if (!QDir().mkpath(root_ + "/media") || !QDir().mkpath(root_ + "/thumbs")) throw Error("Não foi possível criar a biblioteca.");
    lock_ = std::make_unique<QLockFile>(root_ + "/library.lock");
    if (!lock_->tryLock(0)) throw Error("A biblioteca já está aberta em outra instância. Feche-a antes de continuar.");
    QFile f(root_ + "/library.json");
    if (f.exists()) {
        if (!f.open(QIODevice::ReadOnly) || f.size() > 32 * 1024 * 1024) throw Error("Não foi possível ler a biblioteca (limite: 32 MB).");
        QJsonParseError e;
        const auto d = QJsonDocument::fromJson(f.readAll(), &e);
        if (e.error != QJsonParseError::NoError || !d.isObject() || !QList<int>{1,2}.contains(d.object()["schema"].toInt()) || !d.object()["projects"].isArray())
            throw Error("Biblioteca inválida. O arquivo original foi preservado; restaure seu backup.");
        data_ = d.object();
        f.close(); // Windows must release the read handle before QSaveFile replaces this file.
        QSet<QString> projectIds;
        const QRegularExpression safeId("^[A-Za-z0-9_-]{1,128}$"),safeKey("^[a-f0-9]{64}$");
        for(const auto &v:data_["projects"].toArray()){
            const auto p=v.toObject();const auto id=p["id"].toString();const auto cats=p["categories"].toArray();
            if(!safeId.match(id).hasMatch()||projectIds.contains(id)||!safeKey.match(p["key"].toString()).hasMatch()||p["name"].toString().trimmed().isEmpty()||p["name"].toString().size()>120||cats.isEmpty()||cats.size()>56||p["media"].toArray().size()>1000||p["width"].toInt()<320||p["width"].toInt()>7680||p["height"].toInt()<180||p["height"].toInt()>4320)throw Error("Projeto inválido na biblioteca. Arquivo original preservado.");
            projectIds.insert(id);QSet<QString> mediaIds,categoryNames;
            for(const auto &cat:cats){const auto n=cat.toString();if(n.trimmed().isEmpty()||n.size()>120||categoryNames.contains(n))throw Error("Categoria inválida na biblioteca.");categoryNames.insert(n);}
            for(const auto &entry:p["media"].toArray()){const auto m=entry.toObject();const auto mid=m["id"].toString();if(!safeId.match(mid).hasMatch()||mediaIds.contains(mid)||!categoryNames.contains(m["category"].toString()))throw Error("Elemento inválido na biblioteca.");mediaIds.insert(mid);validatedLayout({},m["draft"].toObject());if(m["live"].isObject())validatedLayout({},m["live"].toObject());}
        }
        if(projectIds.size()>200||(!data_["active"].toString().isEmpty()&&!projectIds.contains(data_["active"].toString())))throw Error("Seleção de projeto inválida. Biblioteca preservada.");
        if(data_["schema"].toInt()==1){
            // Preserve the exact original before the first schema conversion.
            auto backup=root_+"/library.schema1.backup.json";if(QFile::exists(backup))backup=root_+"/library.schema1."+uuid()+".backup.json";
            if(!QFile::copy(f.fileName(),backup))throw Error("Não foi possível criar backup da biblioteca anterior.");
            auto next=data_;next["schema"]=2;auto projects=next["projects"].toArray();
            for(int i=0;i<projects.size();++i){auto p=projects[i].toObject();QJsonObject ids;
                auto categories=p["categories"].toArray();for(const auto &canonical:QStringList{"Logos","Tarjetas","Banners","Overlays"})if(!categories.contains(canonical)&&categories.size()<50)categories.append(canonical);
                for(const auto &c:categories)ids[c.toString()]=uuid();
                p["categories"]=categories;p["categoryIds"]=ids;projects[i]=p;}
            next["projects"]=projects;commit(next);
        }
        if(data_["panelVersion"].toInt()<3){
            auto backup=root_+"/library.before-panel3.backup.json";if(QFile::exists(backup))backup=root_+"/library.before-panel3."+uuid()+".backup.json";
            if(!QFile::copy(f.fileName(),backup))throw Error("Falha ao preservar biblioteca antes da atualização do painel.");
            auto next=data_;auto projects=next["projects"].toArray();
            for(int i=0;i<projects.size();++i){auto p=projects[i].toObject();auto cats=p["categories"].toArray();auto ids=p["categoryIds"].toObject();
                for(const auto &cat:panelCategories())if(!cats.contains(cat)){if(cats.size()>=56)throw Error("Limite de categorias ao atualizar o painel. Biblioteca preservada; reorganize as categorias antigas.");cats.append(cat);ids[cat]=uuid();}
                // Legacy categories and original names remain available in the content manager.
                p["categories"]=cats;p["categoryIds"]=ids;projects[i]=p;
            }next["projects"]=projects;next["panelVersion"]=3;commit(next);
        }
        if(data_["positionVersion"].toInt()<1){
            auto backup=root_+"/library.before-category-positions.backup.json";if(QFile::exists(backup))backup=root_+"/library.before-category-positions."+uuid()+".backup.json";
            if(!QFile::copy(f.fileName(),backup))throw Error("Não foi possível preservar a biblioteca antes de migrar as posições.");
            auto next=data_;auto projects=next["projects"].toArray();for(int i=0;i<projects.size();++i){auto p=projects[i].toObject();auto layouts=p["categoryLayouts"].toObject(),frames=p["categoryFrames"].toObject();for(const auto &cat:panelCategories())if(!layouts.contains(cat)){layouts[cat]=categoryLayout(p["id"].toString(),cat);frames[cat]=QJsonObject{{"width",p["width"]},{"height",p["height"]}};}p["categoryLayouts"]=layouts;p["categoryFrames"]=frames;projects[i]=p;}next["projects"]=projects;next["positionVersion"]=1;commit(next);
        }
    } else {
        data_ = {{"schema",2},{"panelVersion",3},{"positionVersion",1},{"port",28741},{"active",""},{"projects",QJsonArray{}}};
        createProject("Minha transmissão");
    }
    scoreboards();
    auto *clock=new QTimer(this);clock->setInterval(1000);connect(clock,&QTimer::timeout,this,[this]{for(const auto &v:data_["projects"].toArray()){const auto p=v.toObject();if(!p["scoreboardData"].toObject()["clockRunning"].toBool())continue;for(const auto &entry:p["media"].toArray()){const auto m=entry.toObject();if(m["mime"]=="application/x-jp-scoreboard"&&m["live"].isObject()){emit changed();return;}}}});clock->start();
    auto *cacheCleanup=new QTimer(this);cacheCleanup->setInterval(60000);connect(cacheCleanup,&QTimer::timeout,this,[this]{const auto cutoff=QDateTime::currentDateTime().addSecs(-300);for(const auto &info:QDir(root_+"/scoreboard-cache").entryInfoList({"*.png"},QDir::Files))if(info.lastModified()<cutoff)QFile::remove(info.absoluteFilePath());});cacheCleanup->start();
}
void Store::commit(const QJsonObject &next) {
    const auto bytes = QJsonDocument(next).toJson();
    if (bytes.size() > 32 * 1024 * 1024) throw Error("A biblioteca atingiu o limite de 32 MB de metadados.");
    QSaveFile f(root_ + "/library.json");
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
        throw Error("Falha ao salvar. Verifique espaço livre e permissões da biblioteca.");
    data_ = next;
    emit changed();
}
QJsonObject Store::project(const QString &id) const {
    for (const auto &p : data_["projects"].toArray()) if (p.toObject()["id"].toString() == id) return p.toObject();
    throw Error("Transmissão não encontrada.");
}
QJsonObject Store::projectByKey(const QString &key) const {
    if (key.size() != 64) throw Error("Overlay não encontrado.");
    for (const auto &p : data_["projects"].toArray()) if (p.toObject()["key"].toString() == key) return p.toObject();
    throw Error("Overlay não encontrado.");
}
void Store::replaceProject(const QJsonObject &p) {
    auto list = data_["projects"].toArray();
    for (int i=0; i<list.size(); ++i) if (list[i].toObject()["id"] == p["id"]) {
        list[i] = p; auto next = data_; next["projects"] = list; commit(next); return;
    }
    throw Error("Transmissão não encontrada.");
}
QString Store::createProject(const QString &name) {
    auto list = data_["projects"].toArray();
    if (list.size() >= 200) throw Error("Limite de 200 transmissões atingido.");
    const auto id = uuid();
    QJsonArray categories{"Logos","Banner","QR-Codes","Vinhetas","Abertura","Vídeos"};QJsonObject ids;
    for(const auto &c:categories)ids[c.toString()]=uuid();
    QJsonObject positions,frames;for(const auto &cat:panelCategories()){positions[cat]=defaultPosition(cat,{1920,1080});frames[cat]=QJsonObject{{"width",1920},{"height",1080}};}
    list.append(QJsonObject{{"id",id},{"key",randomKey()},{"name",nameChecked(name)},
        {"width",1920},{"height",1080},{"categories",categories},{"categoryIds",ids},{"categoryLayouts",positions},{"categoryFrames",frames}, {"media",QJsonArray{}}});
    auto next = data_; next["projects"] = list; next["active"] = id; commit(next); return id;
}
void Store::renameProject(const QString &id, const QString &name) { auto p = project(id); p["name"] = nameChecked(name); replaceProject(p); }
void Store::selectProject(const QString &id) { project(id); auto next = data_; next["active"] = id; commit(next); }
void Store::removeProject(const QString &id) {
    const auto p=project(id);for(const auto &v:p["media"].toArray())if(v.toObject()["live"].isObject())throw Error("Retire os conteúdos publicados antes de excluir o projeto."); auto list = data_["projects"].toArray();
    for (int i=0; i<list.size(); ++i) if (list[i].toObject()["id"].toString() == id) { list.removeAt(i); break; }
    auto next = data_; next["projects"] = list;
    if (next["active"].toString() == id) next["active"] = list.isEmpty() ? "" : list[0].toObject()["id"].toString();
    commit(next); // Media files retained for recovery; no recursive filesystem deletion.
}
void Store::categories(const QString &id, const QJsonArray &values) {
    if (values.isEmpty() || values.size() > 56) throw Error("Use entre 1 e 56 categorias.");
    QStringList seen; QJsonArray clean;
    for (const auto &v : values) { const auto s=nameChecked(v.toString()); if (seen.contains(s)) throw Error("Categoria duplicada."); seen << s; clean.append(s); }
    auto p=project(id); auto media=p["media"].toArray();
    // Rename by position so existing items follow edited category names.
    const auto old = p["categories"].toArray();
    for (int i=0; i<media.size(); ++i) { auto m=media[i].toObject(); int index=-1;
        for (int j=0;j<old.size();++j) if (old[j] == m["category"]) index=j;
        m["category"] = clean[qBound(0,index,int(clean.size())-1)]; media[i]=m;
    }
    QJsonObject ids;const auto previous=p["categoryIds"].toObject();
    for(int i=0;i<clean.size();++i){const auto oldName=i<old.size()?old[i].toString():QString();ids[clean[i].toString()]=previous.value(oldName).toString(uuid());}
    p["categoryIds"]=ids;p["categories"]=clean; p["media"]=media; replaceProject(p);
}
static QJsonObject prepareFile(const QString &root,const QString &path){
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly) || in.size() < 12 || in.size() > 256LL*1024*1024) throw Error("Arquivo ilegível, vazio ou maior que 256 MB.");
    const auto mime=mediaType(in.peek(4096));
    if (mime.isEmpty()) throw Error("Formato inválido. Use PNG, JPEG, WebP, GIF, WebM ou MP4.");
    const auto id=uuid(); const auto rel="media/"+id+"."+mime.section('/',1).replace("jpeg","jpg");
    QSaveFile out(root+"/"+rel);QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!out.open(QIODevice::WriteOnly)) throw Error("Não foi possível guardar a mídia.");
    while (!in.atEnd()) {if(QThread::currentThread()->isInterruptionRequested())throw Error("Importação cancelada.");const auto b=in.read(1024*1024); if (b.isEmpty() || out.write(b)!=b.size()) throw Error("Erro ao copiar a mídia.");hash.addData(b);}
    if (!out.commit()) throw Error("Falha ao guardar mídia na biblioteca.");
    int w=960,h=540;
    if (mime.startsWith("image/")) {
        QImageReader reader(root+"/"+rel); const auto size=reader.size();
        if (size.isValid()) { if (qint64(size.width())*size.height()>64LL*1024*1024) { QFile::remove(root+"/"+rel); throw Error("Imagem acima do limite de 64 megapixels."); }
            w=size.width();h=size.height(); }
    }
    return {{"id",id},{"file",rel},{"mime",mime},{"bytes",double(in.size())},{"width",w},{"height",h},{"hash",QString::fromLatin1(hash.result().toHex())},{"name",QFileInfo(path).completeBaseName().left(120)}};
}
QString Store::adoptFile(const QString &pid,const QJsonObject &prepared,const QString &category,const QString &origin){
    auto p=project(pid);auto list=p["media"].toArray();if(list.size()>=1000)throw Error("Limite de 1.000 mídias atingido.");if(!p["categories"].toArray().contains(category))throw Error("Categoria não encontrada.");
    const auto id=prepared["id"].toString(),mime=prepared["mime"].toString();const auto rel="media/"+prepared["hash"].toString()+"."+mime.section('/',1).replace("jpeg","jpg");
    const bool created=!QFile::exists(root_+"/"+rel);if(created&&!QFile::rename(root_+"/"+prepared["file"].toString(),root_+"/"+rel))throw Error("Falha ao finalizar cache.");
    int w=prepared["width"].toInt(),h=prepared["height"].toInt();const int cw=p["width"].toInt(1920),ch=p["height"].toInt(1080);
    const double scale=std::min({1.0,double(cw)/w,double(ch)/h}); w=qMax(1,int(w*scale));h=qMax(1,int(h*scale));
    int x=(cw-w)/2,y=(ch-h)/2;
    if(category=="Logos"){x=qRound(cw*.04);y=qRound(ch*.04);}
    if(category=="Tarjetas"||category=="Rodapés"||category=="Banners"||category=="Banner")y=qMax(0,ch-h-qRound(ch*.05));
    QJsonObject draft{{"x",x},{"y",y},{"width",w},{"height",h},{"z",list.size()},
        {"transition","fade"},{"duration",300},{"loop",true}};
    QJsonObject m{{"id",id},{"name",prepared["name"]},{"category",category},{"file",rel},
        {"mime",mime},{"bytes",prepared["bytes"]},{"origin",origin},{"draft",draft},{"live",QJsonValue::Null},{"generation",0}};
    list.append(m);p["media"]=list;
    try { replaceProject(p); } catch (...) { if(created)QFile::remove(root_+"/"+rel); throw; }
    return id;
}
QString Store::importFile(const QString &pid,const QString &path,const QString &category,const QString &origin){
    project(pid);const auto prepared=prepareFile(root_,path);try{const auto id=adoptFile(pid,prepared,category,origin.isEmpty()?path:origin);QFile::remove(root_+"/"+prepared["file"].toString());return id;}catch(...){QFile::remove(root_+"/"+prepared["file"].toString());throw;}
}
void Store::importFileAsync(const QString &pid,const QString &path,const QString &category,const QString &origin,bool removeSource){
    project(pid);if(importQueue_.size()+importing_>=1000)throw Error("Fila de importação cheia.");importQueue_.append({pid,path,category,origin,removeSource?"remove":"keep"});startNextImport();
}
void Store::startNextImport(){
    if(closing_||importQueue_.isEmpty()||importing_>=2)return;const auto job=importQueue_.takeFirst();++importing_;
    const auto pid=job[0],path=job[1],category=job[2],origin=job[3],replacement=job.value(5);
    struct Result{QJsonObject prepared;QString error,root,source;~Result(){if(!prepared.isEmpty())QFile::remove(root+"/"+prepared["file"].toString());if(!source.isEmpty())QFile::remove(source);}};
    auto result=std::make_shared<Result>();result->root=root_;if(job[4]=="remove")result->source=path;
    auto *thread=QThread::create([result,path]{try{result->prepared=prepareFile(result->root,path);}catch(const std::exception &e){result->error=QString::fromUtf8(e.what());}});thread->setParent(this);
    connect(thread,&QThread::finished,this,[this,result,pid,path,category,origin,replacement]{try{if(!result->error.isEmpty())throw Error(result->error);if(!origin.isEmpty()){auto name=QFileInfo(QUrl(origin).path()).completeBaseName().left(120);result->prepared["name"]=name.isEmpty()?"Mídia por URL":name;}if(replacement.isEmpty()){adoptFile(pid,result->prepared,category,origin.isEmpty()?path:origin);emit importFinished("Mídia guardada no cache: "+result->prepared["name"].toString(),false);}else{adoptReplacement(pid,replacement,result->prepared,path);result->prepared={};emit importFinished("Arquivo substituído; posição e identificação preservadas.",false);}}catch(const std::exception &e){emit importFinished(QString::fromUtf8(e.what()),true);}--importing_;startNextImport();});
    connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();startNextImport();
}
void Store::updateMedia(const QString &pid, const QString &id, const QJsonObject &patch) {
    auto p=project(pid);auto list=p["media"].toArray();
    for (int i=0;i<list.size();++i) { auto m=list[i].toObject(); if (m["id"].toString()!=id) continue;
        if (patch.contains("name")) m["name"]=nameChecked(patch["name"].toString());
        if(patch.contains("group")){const auto group=patch["group"].toString().trimmed();if(group.size()>120)throw Error("Nome de grupo acima de 120 caracteres.");m["group"]=group;}
        if (patch.contains("category")) { if (!p["categories"].toArray().contains(patch["category"])) throw Error("Categoria inválida."); m["category"]=patch["category"]; }
        auto draft=validatedLayout(m["draft"].toObject(),patch["draft"].toObject());
        if(patch.contains("text")){const auto text=patch["text"].toObject();
            for(const auto &v:text)if(!v.isString()||v.toString().size()>500)throw Error("Campo de texto inválido (máximo 500 caracteres).");
            draft["text"]=text;}
        if(patch.contains("favorite"))m["favorite"]=patch["favorite"].toBool();
        if(patch.contains("disabled")){if(m["live"].isObject()&&patch["disabled"].toBool())throw Error("Retire do ar antes de desativar.");m["disabled"]=patch["disabled"].toBool();}
        m["draft"]=draft; list[i]=m; p["media"]=list; replaceProject(p);return;
    }
    throw Error("Mídia não encontrada.");
}
void Store::setLive(const QString &pid, const QString &id, bool on) {
    auto p=project(pid);auto list=p["media"].toArray();
    for(int i=0;i<list.size();++i) {auto m=list[i].toObject();if(m["id"].toString()!=id) continue;
        const bool wasOn=m["live"].isObject();
        if(on) {if(m["disabled"].toBool())throw Error("Elemento desativado.");m["live"]=m["draft"]; if(!wasOn) m["generation"]=m["generation"].toInt()+1;} else m["live"]=QJsonValue::Null;
        list[i]=m;p["media"]=list;replaceProject(p);return;
    } throw Error("Mídia não encontrada.");
}
void Store::updateScoreboardPosition(const QString &pid,const QString &id,const QJsonObject &layout,bool applyLive){
    auto p=project(pid);auto list=p["media"].toArray();for(int i=0;i<list.size();++i){auto m=list[i].toObject();if(m["id"]!=id)continue;if(m["mime"]!="application/x-jp-scoreboard")throw Error("Posição individual disponível somente para scoreboard.");
        m["draft"]=validatedLayout(m["draft"].toObject(),layout);if(applyLive&&m["live"].isObject())m["live"]=validatedLayout(m["live"].toObject(),layout);list[i]=m;p["media"]=list;replaceProject(p);return;
    }throw Error("Scoreboard não encontrado no evento.");
}
void Store::take(const QString &pid,const QString &id){
    auto p=project(pid);auto list=p["media"].toArray();QJsonObject selected;
    for(const auto &v:list)if(v.toObject()["id"].toString()==id)selected=v.toObject();
    if(selected.isEmpty())throw Error("Mídia não encontrada.");
    if(selected["disabled"].toBool())throw Error("Elemento desativado.");
    for(int i=0;i<list.size();++i){auto m=list[i].toObject();
        if(m["id"].toString()==id){m["live"]=effectiveLayout(p,m);m["generation"]=m["generation"].toInt()+1;}
        else if(m["mime"]=="application/x-jp-scoreboard"?selected["mime"]=="application/x-jp-scoreboard":selected["mime"]!="application/x-jp-scoreboard"&&panelCategory(m["category"].toString(),m["mime"].toString())==panelCategory(selected["category"].toString(),selected["mime"].toString()))m["live"]=QJsonValue::Null;
        list[i]=m;
    }p["media"]=list;replaceProject(p);
}
void Store::replaceMedia(const QString &pid,const QString &id,const QString &path){
    const auto prepared=prepareFile(root_,path);try{adoptReplacement(pid,id,prepared,path);}catch(...){QFile::remove(root_+"/"+prepared["file"].toString());throw;}
}
void Store::replaceMediaAsync(const QString &pid,const QString &id,const QString &path){
    project(pid);if(importQueue_.size()+importing_>=1000)throw Error("Fila de importação cheia.");importQueue_.append({pid,path,"","","keep",id});startNextImport();
}
void Store::adoptReplacement(const QString &pid,const QString &id,const QJsonObject &prepared,const QString &path){
    auto p=project(pid);auto list=p["media"].toArray();int index=-1;
    for(int i=0;i<list.size();++i)if(list[i].toObject()["id"].toString()==id)index=i;
    if(index<0)throw Error("Mídia não encontrada.");
    auto m=list[index].toObject();if(m["live"].isObject())throw Error("Execute OUT e aguarde a confirmação antes de substituir o arquivo.");
    m["file"]=prepared["file"];m["mime"]=prepared["mime"];m["bytes"]=prepared["bytes"];m["origin"]=path;m.remove("thumbnail");list[index]=m;p["media"]=list;
    replaceProject(p);
}
void Store::clearLive(const QString &id) {auto p=project(id);auto list=p["media"].toArray(); for(int i=0;i<list.size();++i){auto m=list[i].toObject();m["live"]=QJsonValue::Null;list[i]=m;}p["media"]=list;replaceProject(p);}
void Store::removeMedia(const QString &pid,const QString &id) {auto p=project(pid);auto list=p["media"].toArray(); for(int i=0;i<list.size();++i)if(list[i].toObject()["id"].toString()==id){if(list[i].toObject()["live"].isObject())throw Error("Retire o conteúdo do ar antes de excluir o cadastro.");list.removeAt(i);p["media"]=list;replaceProject(p);return;}throw Error("Mídia não encontrada.");}
void Store::saveThumbnail(const QString &pid,const QString &id,const QByteArray &png) {
    if(png.size()>512*1024 || mediaType(png)!="image/png")throw Error("Miniatura inválida.");
    QBuffer buffer;buffer.setData(png);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer,"PNG");
    const auto size=reader.size();if(!size.isValid()||size.width()>640||size.height()>360)throw Error("Miniatura acima de 640 × 360.");
    const auto image=reader.read();if(image.isNull())throw Error("Miniatura inválida.");
    auto p=project(pid);auto list=p["media"].toArray();for(int i=0;i<list.size();++i){auto m=list[i].toObject();if(m["id"].toString()!=id)continue;
        const auto rel="thumbs/"+id+".png";QSaveFile f(root_+"/"+rel);if(!f.open(QIODevice::WriteOnly)||f.write(png)!=png.size()||!f.commit())throw Error("Falha ao salvar miniatura.");
        m["thumbnail"]=rel;list[i]=m;p["media"]=list;replaceProject(p);return;}throw Error("Mídia não encontrada.");
}
QString Store::mediaPath(const QJsonObject &m,bool thumb) const {
    const auto rel=m[thumb?"thumbnail":"file"].toString();
    if(rel.isEmpty()||rel.contains("..")||rel.contains('\\')||QDir::isAbsolutePath(rel)||!(rel.startsWith("media/")||rel.startsWith("thumbs/")))throw Error("Arquivo indisponível: o cadastro aponta para um caminho inválido fora da biblioteca de mídia.");
    const auto path=QFileInfo(root_+"/"+rel).canonicalFilePath();const auto base=QFileInfo(root_).canonicalFilePath()+"/";
    if(path.isEmpty()||!path.startsWith(base))throw Error("Mídia ausente da biblioteca.");return path;
}
QJsonObject Store::liveState(const QString &key) const {
    const auto p=projectByKey(key);QJsonArray live;
    for(const auto &v:p["media"].toArray()){auto m=v.toObject();if(!m["live"].isObject())continue;
        const bool board=m["mime"]=="application/x-jp-scoreboard";
        live.append(QJsonObject{{"id",m["id"]},{"name",m["name"]},{"mime",board?QJsonValue("image/png"):m["mime"]},{"layout",m["live"]},{"generation",m["generation"]},
            {"url","/media/"+key+"/"+m["id"].toString()+(board?"?v="+QString::number(QDateTime::currentMSecsSinceEpoch()/1000):QString())}});}
    return {{"width",p["width"]},{"height",p["height"]},{"items",live}};
}
quint16 Store::preferredPort()const{return quint16(qBound(1024,data_["port"].toInt(28741),65535));}
void Store::port(quint16 p){if(preferredPort()==p)return;auto next=data_;next["port"]=p;commit(next);}
}
