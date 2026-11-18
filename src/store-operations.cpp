#include "store.hpp"
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QUuid>
#include <QSet>
#include <QMap>
#include <QImageReader>
#include <QThread>
#include <cmath>

namespace jp {
static QString newId(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
static const QStringList numericFields{"x","y","width","height","z","duration","scale","rotation","opacity","cropLeft","cropRight","cropTop","cropBottom"};
static const QStringList syncFields=numericFields+QStringList{"anchor","alignment"};
QJsonObject Store::validatedLayout(const QJsonObject &base,const QJsonObject &patch){
    auto result=base;
    for(auto it=patch.begin();it!=patch.end();++it){const auto k=it.key();
        if(numericFields.contains(k)){
            double lo=-10000,hi=10000;
            if(k=="width"||k=="height"){lo=1;hi=16384;}
            else if(k=="scale"){lo=.01;hi=20;}
            else if(k=="opacity"){lo=0;hi=100;}
            else if(k=="rotation"){lo=-360;hi=360;}
            else if(k.startsWith("crop")){lo=0;hi=16384;}
            else if(k=="duration"){lo=0;hi=3000;}
            const auto n=it.value().toDouble(NAN);
            if(!std::isfinite(n)||n<lo||n>hi)throw Error("Transformação fora do limite: "+k);
            result[k]=n;
        }else if(k=="transition"){
            if(!QStringList{"none","fade","left","right","bottom","scale"}.contains(it.value().toString()))throw Error("Transição inválida.");result[k]=it.value();
        }else if(k=="loop")result[k]=it.value().toBool();
        else if(k=="endBehavior"){if(!QStringList{"hold","out"}.contains(it.value().toString()))throw Error("Comportamento final inválido.");result[k]=it.value();}
        else if(k=="anchor"||k=="alignment"){
            if(!QStringList{"top-left","top-center","top-right","center-left","center","center-right","bottom-left","bottom-center","bottom-right"}.contains(it.value().toString()))throw Error("Âncora/alinhamento inválido.");result[k]=it.value();
        }else if(k=="text"){
            if(!it.value().isObject())throw Error("Texto inválido.");
            for(const auto &v:it.value().toObject())if(!v.isString()||v.toString().size()>500)throw Error("Texto acima de 500 caracteres.");result[k]=it.value();
        }else throw Error("Propriedade de preparação desconhecida: "+k);
    }
    if(result.value("cropLeft").toDouble()+result.value("cropRight").toDouble()>=result.value("width").toDouble(1)||
       result.value("cropTop").toDouble()+result.value("cropBottom").toDouble()>=result.value("height").toDouble(1))throw Error("Recorte remove toda a peça.");
    return result;
}
void Store::renameCategory(const QString &pid,const QString &name,const QString &replacement){
    const auto clean=replacement.trimmed();auto p=project(pid);auto cats=p["categories"].toArray();
    if(clean.isEmpty()||clean.size()>120||cats.contains(clean))throw Error("Use um nome de categoria único, de 1 a 120 caracteres.");
    int index=-1;for(int i=0;i<cats.size();++i)if(cats[i].toString()==name)index=i;if(index<0)throw Error("Categoria não encontrada.");cats[index]=clean;
    auto ids=p["categoryIds"].toObject();const auto id=ids.take(name);ids[clean]=id;auto media=p["media"].toArray();
    for(int i=0;i<media.size();++i){auto m=media[i].toObject();if(m["category"].toString()==name){m["category"]=clean;media[i]=m;}}
    p["categories"]=cats;p["categoryIds"]=ids;p["media"]=media;replaceProject(p);
}
void Store::addCategory(const QString &pid,const QString &name){
    auto p=project(pid);auto cats=p["categories"].toArray();const auto n=name.trimmed();
    if(n.isEmpty()||n.size()>120||cats.contains(n)||cats.size()>=56)throw Error("Categoria inválida, repetida ou limite atingido.");
    cats.append(n);auto ids=p["categoryIds"].toObject();ids[n]=newId();p["categories"]=cats;p["categoryIds"]=ids;replaceProject(p);
}
void Store::resolution(const QString &pid,int width,int height){
    if(width<320||height<180||width>7680||height>4320)throw Error("Resolução permitida: 320 × 180 a 7680 × 4320.");
    auto p=project(pid);for(const auto &v:p["media"].toArray())if(v.toObject()["live"].isObject())throw Error("Retire as peças do ar antes de alterar a resolução.");
    p["width"]=width;p["height"]=height;replaceProject(p);
}
QString Store::duplicateMedia(const QString &pid,const QString &id){
    auto p=project(pid);auto media=p["media"].toArray();if(media.size()>=1000)throw Error("Limite de mídias atingido.");
    for(const auto &v:media){auto m=v.toObject();if(m["id"].toString()!=id)continue;const auto next=newId();
        m["id"]=next;m["name"]=m["name"].toString().left(110)+" (cópia)";m["live"]=QJsonValue::Null;m["generation"]=0;media.append(m);p["media"]=media;replaceProject(p);return next;}
    throw Error("Mídia não encontrada.");
}
QString Store::createTitle(const QString &pid,const QString &name,const QJsonObject &text){
    auto p=project(pid);auto media=p["media"].toArray();const auto n=name.trimmed();
    if(n.isEmpty()||n.size()>120||media.size()>=1000)throw Error("Nome inválido ou biblioteca cheia.");
    auto cats=p["categories"].toArray();if(!cats.contains("Tarjetas")){if(cats.size()>=56)throw Error("Limite de categorias.");cats.append("Tarjetas");p["categories"]=cats;auto ids=p["categoryIds"].toObject();ids["Tarjetas"]=newId();p["categoryIds"]=ids;}
    const int w=p["width"].toInt(),h=p["height"].toInt();const auto id=newId();
    auto draft=validatedLayout({{"x",w*.05},{"y",h*.78},{"width",w*.7},{"height",h*.16},{"z",media.size()},{"transition","fade"},{"duration",300},{"loop",true}},{{"text",text}});
    media.append(QJsonObject{{"id",id},{"name",n},{"category","Tarjetas"},{"mime","application/x-jp-title"},{"bytes",0},{"draft",draft},{"live",QJsonValue::Null},{"generation",0}});
    p["media"]=media;replaceProject(p);return id;
}
QJsonArray Store::synchronize(const QString &pid,const QString &source,const QStringList &targets,const QStringList &properties){
    if(targets.isEmpty()||properties.isEmpty())throw Error("Selecione destinos e propriedades.");
    auto p=project(pid);auto media=p["media"].toArray();QJsonObject origin;QSet<QString> wanted;
    for(const auto &v:media)if(v.toObject()["id"].toString()==source)origin=v.toObject();
    if(origin.isEmpty())throw Error("Origem não encontrada.");
    for(const auto &field:properties)if(!syncFields.contains(field))throw Error("Propriedade incompatível: "+field);
    for(const auto &id:targets){if(id==source)throw Error("Origem não pode ser destino.");wanted.insert(id);}
    const auto layout=origin["draft"].toObject();QJsonArray changes;
    for(int i=0;i<media.size();++i){auto m=media[i].toObject();const auto id=m["id"].toString();if(!wanted.remove(id))continue;
        const auto before=m["draft"].toObject();QJsonObject from,to;
        for(const auto &field:properties){from[field]=before.contains(field)?before[field]:QJsonValue::Null;
            to[field]=layout.contains(field)?layout[field]:QJsonValue(field=="scale"?1:field=="opacity"?100:0);
            if((field=="anchor"||field=="alignment")&&!layout.contains(field))to[field]=field=="alignment"?"center":"top-left";}
        m["draft"]=validatedLayout(before,to);media[i]=m;changes.append(QJsonObject{{"id",id},{"before",from},{"after",to}});
    }
    if(!wanted.isEmpty())throw Error("Um dos destinos não existe mais. Nenhuma alteração aplicada.");
    p["media"]=media;replaceProject(p);return changes;
}
void Store::restoreLayouts(const QString &pid,const QJsonArray &changes,bool undo){
    auto p=project(pid);auto media=p["media"].toArray();
    for(const auto &v:changes){const auto change=v.toObject();bool found=false;
        for(int i=0;i<media.size();++i){auto m=media[i].toObject();if(m["id"]!=change["id"])continue;found=true;auto layout=m["draft"].toObject();
            const auto expected=change[undo?"after":"before"].toObject(),replacement=change[undo?"before":"after"].toObject();
            for(auto it=expected.begin();it!=expected.end();++it){const QJsonValue actual=layout.contains(it.key())?layout.value(it.key()):QJsonValue(QJsonValue::Null);
                if(actual!=it.value())throw Error("A preparação foi modificada depois da sincronização. Desfazer/refazer cancelado.");}
            for(auto it=replacement.begin();it!=replacement.end();++it){if(it.value().isNull())layout.remove(it.key());else layout[it.key()]=it.value();}
            m["draft"]=validatedLayout({},layout);media[i]=m;break;}
        if(!found)throw Error("Destino removido; nenhuma alteração aplicada.");
    }
    p["media"]=media;replaceProject(p);
}
void Store::saveTemplate(const QString &pid,const QString &name,const QJsonObject &layout){
    const auto n=name.trimmed();if(n.isEmpty()||n.size()>120)throw Error("Nome de modelo inválido.");
    auto p=project(pid);auto models=p["templates"].toObject();if(models.size()>=100&&!models.contains(n))throw Error("Limite de 100 modelos.");
    QJsonObject clean;for(auto it=layout.begin();it!=layout.end();++it)if(syncFields.contains(it.key()))clean[it.key()]=it.value();
    if(clean.isEmpty())throw Error("Modelo sem propriedades.");models[n]=validatedLayout({{"width",16384},{"height",16384}},clean);
    // Only the selected mask is stored, without synthetic validation defaults.
    QJsonObject mask;for(auto it=clean.begin();it!=clean.end();++it)mask[it.key()]=clean[it.key()];models[n]=mask;
    p["templates"]=models;replaceProject(p);
}
void Store::applyTemplate(const QString &pid,const QString &name,const QStringList &targets){
    auto p=project(pid);const auto layout=p["templates"].toObject()[name].toObject();if(layout.isEmpty())throw Error("Modelo não encontrado.");
    auto media=p["media"].toArray();QSet<QString> remaining(targets.begin(),targets.end());
    for(int i=0;i<media.size();++i){auto m=media[i].toObject();if(!remaining.remove(m["id"].toString()))continue;m["draft"]=validatedLayout(m["draft"].toObject(),layout);media[i]=m;}
    if(!remaining.isEmpty())throw Error("Destino inexistente.");p["media"]=media;replaceProject(p);
}
static void exportFiles(QJsonObject p,const QString &root,const QString &directory){
    const QDir dir(directory);if(QFile::exists(dir.filePath("project.jpgfx.json")))throw Error("Escolha uma pasta sem projeto exportado.");
    if(!QDir().mkpath(dir.filePath("media")))throw Error("Não foi possível criar a pasta.");auto media=p["media"].toArray();QMap<QString,QString> logoPaths;
    for(int i=0;i<media.size();++i){auto m=media[i].toObject();m["live"]=QJsonValue::Null;m.remove("thumbnail");
        if(QThread::currentThread()->isInterruptionRequested())throw Error("Exportação cancelada.");
        if(m["mime"]!="application/x-jp-title"&&m["mime"]!="application/x-jp-scoreboard"){const auto file=m["file"].toString();if(!file.startsWith("media/")||file.contains("..")||file.contains('\\'))throw Error("Caminho de mídia inválido.");const auto src=QFileInfo(root+"/"+file).canonicalFilePath();if(src.isEmpty()||!src.startsWith(QFileInfo(root).canonicalFilePath()+"/"))throw Error("Mídia fora da biblioteca.");const auto rel="media/"+m["id"].toString()+"."+QFileInfo(src).suffix();
            if(!QFile::copy(src,dir.filePath(rel)))throw Error("Falha ao exportar mídia. Use uma pasta vazia.");logoPaths[file]=rel;m["file"]=rel;}
        media[i]=m;}
    p["media"]=media;auto score=p["scoreboardData"].toObject();for(const auto &key:{"team1.logo","team2.logo"})if(score.contains(key))score[key]=logoPaths.value(score[key].toString());p["scoreboardData"]=score;p.remove("key");QSaveFile f(dir.filePath("project.jpgfx.json"));const auto bytes=QJsonDocument(QJsonObject{{"format","jpgfx-project"},{"version",1},{"project",p}}).toJson();
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())throw Error("Falha ao exportar manifest.");
}
static QJsonObject prepareProject(const QString &root,const QString &directory){
    QDir dir(directory);QFile f(dir.filePath("project.jpgfx.json"));
    if(!f.open(QIODevice::ReadOnly)||f.size()>32*1024*1024)throw Error("Manifest inválido ou acima de 32 MB.");
    const auto doc=QJsonDocument::fromJson(f.readAll()).object();if(doc["format"]!="jpgfx-project"||doc["version"]!=1)throw Error("Formato de projeto incompatível.");
    auto p=doc["project"].toObject();const auto name=p["name"].toString().trimmed();auto cats=p["categories"].toArray();
    if(name.isEmpty()||name.size()>120||cats.isEmpty()||cats.size()>56||p["width"].toInt()<320||p["width"].toInt()>7680||p["height"].toInt()<180||p["height"].toInt()>4320)throw Error("Projeto inválido.");
    QSet<QString> names;QJsonObject ids;for(const auto &c:cats){const auto n=c.toString();if(n.trimmed().isEmpty()||n.size()>120||names.contains(n))throw Error("Categoria inválida.");names.insert(n);ids[n]=newId();}
    for(const auto &category:panelCategories())if(!names.contains(category)){if(cats.size()>=56)throw Error("Projeto sem espaço para as categorias do painel.");cats.append(category);names.insert(category);ids[category]=newId();}p["categories"]=cats;
    auto media=p["media"].toArray();if(media.size()>1000)throw Error("Limite de mídias.");
    QStringList copied;QMap<QString,QString> logoPaths;
    try{
        for(int i=0;i<media.size();++i){auto m=media[i].toObject();if(!names.contains(m["category"].toString())||m["name"].toString().trimmed().isEmpty()||m["name"].toString().size()>120)throw Error("Mídia inválida.");
            if(QThread::currentThread()->isInterruptionRequested())throw Error("Importação cancelada.");
            m["draft"]=Store::validatedLayout({},m["draft"].toObject());const auto id=newId();m["id"]=id;m["live"]=QJsonValue::Null;m["generation"]=0;m.remove("thumbnail");
            if(m["mime"]!="application/x-jp-title"&&m["mime"]!="application/x-jp-scoreboard"){
                const auto rel=m["file"].toString();if(!rel.startsWith("media/")||rel.contains("..")||rel.contains('\\')||QDir::isAbsolutePath(rel))throw Error("Caminho de mídia inválido.");
                const auto src=QFileInfo(dir.filePath(rel)).canonicalFilePath(),base=QFileInfo(directory).canonicalFilePath()+"/";
                if(src.isEmpty()||!src.startsWith(base))throw Error("Mídia fora da pasta exportada.");QFile input(src);
                if(!input.open(QIODevice::ReadOnly)||input.size()<12||input.size()>256LL*1024*1024)throw Error("Mídia ilegível ou acima do limite.");
                const auto mime=jp::mediaType(input.peek(4096));if(mime.isEmpty()||mime!=m["mime"].toString())throw Error("Tipo de mídia inválido.");
                if(mime.startsWith("image/")){QImageReader reader(src);const auto size=reader.size();if(size.isValid()&&qint64(size.width())*size.height()>64LL*1024*1024)throw Error("Imagem acima de 64 megapixels.");}
                const auto dest="media/"+id+"."+mime.section('/',1);if(!QFile::copy(src,root+"/"+dest))throw Error("Falha ao copiar mídia importada.");copied<<root+"/"+dest;logoPaths[rel]=dest;m["file"]=dest;m["bytes"]=double(input.size());
            }
            media[i]=m;}
        auto score=p["scoreboardData"].toObject();for(const auto &key:{"team1.logo","team2.logo"})if(score.contains(key))score[key]=logoPaths.value(score[key].toString());p["scoreboardData"]=score;
        p["id"]=newId();p["key"]=randomKey();p["media"]=media;p["categoryIds"]=ids;p["obsBindings"]=QJsonArray{};
        return p;
    }catch(...){for(const auto &path:copied)QFile::remove(path);throw;}
}
static void discardProjectFiles(const QString &root,const QJsonObject &p){for(const auto &v:p["media"].toArray()){const auto file=v.toObject()["file"].toString();if(file.startsWith("media/")&&!file.contains("..")&&!file.contains('\\'))QFile::remove(root+"/"+file);}}
static void verifyProjectScoreboards(const Store &store,const QJsonObject &p){QSet<QString> installed;for(const auto &v:store.scoreboards())installed.insert(v.toObject()["id"].toString());for(const auto &v:p["media"].toArray()){const auto m=v.toObject();if(m["mime"]=="application/x-jp-scoreboard"&&!installed.contains(m["templateId"].toString()))throw Error("Este projeto usa um scoreboard não instalado. Importe o pacote do scoreboard antes do projeto.");}}
void Store::exportProject(const QString &pid,const QString &directory)const{exportFiles(project(pid),root_,directory);}
QString Store::importProject(const QString &directory){
    if(data_["projects"].toArray().size()>=200)throw Error("Limite de projetos.");const auto p=prepareProject(root_,directory);
    try{verifyProjectScoreboards(*this,p);auto list=data_["projects"].toArray();list.append(p);auto next=data_;next["projects"]=list;next["active"]=p["id"];commit(next);return p["id"].toString();}catch(...){discardProjectFiles(root_,p);throw;}
}
void Store::exportProjectAsync(const QString &pid,const QString &directory){
    if(projectJob_)throw Error("Aguarde a importação/exportação de projeto em andamento.");const auto p=project(pid);projectJob_=true;const auto root=root_;auto error=std::make_shared<QString>();
    auto *thread=QThread::create([p,root,directory,error]{try{exportFiles(p,root,directory);}catch(const std::exception &e){*error=QString::fromUtf8(e.what());}});thread->setParent(this);
    connect(thread,&QThread::finished,this,[this,error]{projectJob_=false;emit importFinished(error->isEmpty()?"Projeto exportado com mídias.":*error,!error->isEmpty());});connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
}
void Store::importProjectAsync(const QString &directory){
    if(projectJob_)throw Error("Aguarde a importação/exportação de projeto em andamento.");if(data_["projects"].toArray().size()>=200)throw Error("Limite de projetos.");projectJob_=true;
    struct Result{QString root,error;QJsonObject project;bool adopted=false;~Result(){if(!adopted)discardProjectFiles(root,project);}};auto result=std::make_shared<Result>();result->root=root_;
    auto *thread=QThread::create([result,directory]{try{result->project=prepareProject(result->root,directory);}catch(const std::exception &e){result->error=QString::fromUtf8(e.what());}});thread->setParent(this);
    connect(thread,&QThread::finished,this,[this,result]{projectJob_=false;try{if(!result->error.isEmpty())throw Error(result->error);if(data_["projects"].toArray().size()>=200)throw Error("Limite de projetos.");verifyProjectScoreboards(*this,result->project);auto list=data_["projects"].toArray();list.append(result->project);auto next=data_;next["projects"]=list;next["active"]=result->project["id"];commit(next);result->adopted=true;emit importFinished("Projeto importado; elementos iniciam fora do ar.",false);}catch(const std::exception &e){emit importFinished(QString::fromUtf8(e.what()),true);}});connect(thread,&QThread::finished,thread,&QObject::deleteLater);thread->start();
}
}
