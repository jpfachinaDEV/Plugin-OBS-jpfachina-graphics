#include "store.hpp"
#include <QUuid>
#include <QSet>
#include <cmath>

namespace jp {
QStringList positionFields(){return {"x","y","width","height","scale","rotation","anchor","alignment","cropLeft","cropRight","cropTop","cropBottom"};}
static QSize projectFrame(const QJsonObject &p){return {p["width"].toInt(1920),p["height"].toInt(1080)};}
static QJsonObject positionOnly(const QJsonObject &layout){QJsonObject result;for(const auto &key:positionFields())if(layout.contains(key))result[key]=layout[key];return result;}
QJsonObject positionForFrame(QJsonObject layout,QSize from,QSize to){
    if(!from.isValid()||!to.isValid()||from==to)return layout;
    const double sx=double(to.width())/from.width(),sy=double(to.height())/from.height();
    for(const auto &key:QStringList{"x","width","cropLeft","cropRight"})if(layout.contains(key))layout[key]=layout[key].toDouble()*sx;
    for(const auto &key:QStringList{"y","height","cropTop","cropBottom"})if(layout.contains(key))layout[key]=layout[key].toDouble()*sy;
    return layout;
}
QJsonObject defaultPosition(const QString &category,QSize frame){
    const double w=frame.width(),h=frame.height();
    QJsonObject d{{"x",0},{"y",0},{"width",w},{"height",h},{"scale",1},{"rotation",0},{"anchor","top-left"},{"alignment","center"}};
    if(category=="Logos"){d["x"]=w*.78;d["y"]=h*.04;d["width"]=w*.18;d["height"]=h*.18;}
    else if(category=="Banner"){d["x"]=w*.05;d["y"]=h*.8;d["width"]=w*.9;d["height"]=h*.15;}
    else if(category=="QR-Codes"){d["x"]=w*.79;d["y"]=h*.65;d["width"]=h*.3;d["height"]=h*.3;}
    return d;
}
QJsonObject Store::defaultCategoryLayout(const QString &pid,const QString &category,QSize frame)const{if(!frame.isValid())frame=projectFrame(project(pid));return defaultPosition(category,frame);}
QJsonObject Store::categoryLayout(const QString &pid,const QString &category,QSize frame)const{
    const auto p=project(pid);const auto natural=projectFrame(p);if(!frame.isValid())frame=natural;
    auto layout=p["categoryLayouts"].toObject()[category].toObject();
    if(!layout.isEmpty()){
        const auto reference=p["categoryFrames"].toObject()[category].toObject();const QSize from(reference["width"].toInt(natural.width()),reference["height"].toInt(natural.height()));
        return positionForFrame(layout,from,frame);
    }
    // Legacy projects retain the first prepared placement in each category until explicitly saved.
    for(const auto &v:p["media"].toArray()){const auto m=v.toObject();if(panelCategory(m["category"].toString(),m["mime"].toString())==category)return positionForFrame(positionOnly(m["draft"].toObject()),natural,frame);}
    return defaultCategoryLayout(pid,category,frame);
}
QJsonObject effectiveLayout(const QJsonObject &p,const QJsonObject &m){
    auto result=m["draft"].toObject();const auto category=panelCategory(m["category"].toString(),m["mime"].toString());
    auto position=p["categoryLayouts"].toObject()[category].toObject();if(position.isEmpty())for(const auto &v:p["media"].toArray()){const auto first=v.toObject();if(panelCategory(first["category"].toString(),first["mime"].toString())==category){position=positionOnly(first["draft"].toObject());break;}}
    const auto reference=p["categoryFrames"].toObject()[category].toObject();const auto frame=projectFrame(p);
    position=positionForFrame(position,{reference["width"].toInt(frame.width()),reference["height"].toInt(frame.height())},frame);
    for(auto it=position.begin();it!=position.end();++it)result[it.key()]=it.value();
    return Store::validatedLayout({},result);
}
void Store::saveCategoryLayouts(const QString &pid,const QJsonObject &layouts,QSize frame,bool applyLive){
    if(frame.width()<320||frame.width()>7680||frame.height()<180||frame.height()>4320)throw Error("Enquadramento inválido.");
    auto p=project(pid);auto positions=p["categoryLayouts"].toObject(),frames=p["categoryFrames"].toObject();
    for(auto it=layouts.begin();it!=layouts.end();++it){if(!panelCategories().contains(it.key())||!it.value().isObject())throw Error("Categoria de posicionamento inválida.");
        const auto clean=positionOnly(it.value().toObject());if(clean.size()!=it.value().toObject().size())throw Error("Configuração de posição contém propriedades não permitidas.");
        positions[it.key()]=validatedLayout(defaultCategoryLayout(pid,it.key(),frame),clean);frames[it.key()]=QJsonObject{{"width",frame.width()},{"height",frame.height()}};
    }
    p["categoryLayouts"]=positions;p["categoryFrames"]=frames;
    if(applyLive){auto media=p["media"].toArray();for(int i=0;i<media.size();++i){auto m=media[i].toObject();const auto category=panelCategory(m["category"].toString(),m["mime"].toString());if(!layouts.contains(category)||!m["live"].isObject())continue;
        // Preserve the published playback settings and every other category.
        auto proxy=m;proxy["draft"]=m["live"];m["live"]=effectiveLayout(p,proxy);media[i]=m;
    }p["media"]=media;}
    replaceProject(p);
}
QString Store::duplicateProject(const QString &pid,const QString &name){
    const auto n=name.trimmed();if(n.isEmpty()||n.size()>120)throw Error("Use um nome de 1 a 120 caracteres.");
    auto p=project(pid);const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);p["id"]=id;p["key"]=randomKey();p["name"]=n;
    auto media=p["media"].toArray();for(int i=0;i<media.size();++i){auto m=media[i].toObject();m["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);m["live"]=QJsonValue::Null;m["generation"]=0;media[i]=m;}p["media"]=media;
    QJsonObject ids;for(const auto &cat:p["categories"].toArray())ids[cat.toString()]=QUuid::createUuid().toString(QUuid::WithoutBraces);p["categoryIds"]=ids;
    p["obsBindings"]=QJsonArray{};auto config=p["obsLink"].toObject();config["needsAssociation"]=true;p["obsLink"]=config;
    auto list=data_["projects"].toArray();if(list.size()>=200)throw Error("Limite de projetos atingido.");list.append(p);auto next=data_;next["projects"]=list;commit(next);return id;
}
void Store::obsLink(const QString &pid,const QJsonObject &settings){
    const auto mode=settings["mode"].toString("native");if(mode!="native"&&mode!="websocket")throw Error("Tipo de conexão inválido.");
    const auto port=settings["port"].toInt(4455);if(port<1||port>65535)throw Error("Porta inválida.");
    auto p=project(pid);QJsonObject clean{{"mode",mode},{"host",settings["host"].toString("127.0.0.1").trimmed()},{"port",port},{"tls",settings["tls"].toBool(false)},{"mediaRoot",settings["mediaRoot"].toString().trimmed()}};
    if(clean["host"].toString().isEmpty()||clean["host"].toString().size()>253||clean["host"].toString().contains('@')||clean["host"].toString().contains('/'))throw Error("Informe somente o endereço do OBS, sem senha ou caminho.");
    p["obsLink"]=clean;replaceProject(p);
}
void Store::obsBindings(const QString &pid,const QJsonArray &bindings){auto p=project(pid);if(bindings.size()>1000)throw Error("Limite de associações atingido.");p["obsBindings"]=bindings;replaceProject(p);}
QJsonArray Store::synchronizeCategories(const QString &pid,const QString &source,const QStringList &targets,const QStringList &fields){
    if(!panelCategories().contains(source)||targets.isEmpty()||fields.isEmpty())throw Error("Selecione categoria de origem, destinos e propriedades.");
    for(const auto &field:fields)if(!positionFields().contains(field))throw Error("Propriedade de posição inválida.");
    const auto frame=projectFrame(project(pid));const auto origin=validatedLayout(defaultCategoryLayout(pid,source,frame),categoryLayout(pid,source,frame));QJsonObject layouts;QJsonArray changes;QSet<QString> seen;
    for(const auto &cat:targets){if(!panelCategories().contains(cat)||cat==source||seen.contains(cat))throw Error("Destino de sincronização inválido.");seen.insert(cat);const auto before=validatedLayout(defaultCategoryLayout(pid,cat,frame),categoryLayout(pid,cat,frame));QJsonObject patch;for(const auto &field:fields)patch[field]=origin.value(field).isUndefined()?QJsonValue(0):origin[field];const auto after=validatedLayout(before,patch);layouts[cat]=after;changes.append(QJsonObject{{"category",cat},{"before",before},{"after",after}});}
    saveCategoryLayouts(pid,layouts,frame);return changes;
}
void Store::restoreCategoryLayouts(const QString &pid,const QJsonArray &changes,bool undo){
    const auto frame=projectFrame(project(pid));QJsonObject layouts;for(const auto &v:changes){const auto change=v.toObject();const auto cat=change["category"].toString();const auto expected=change[undo?"after":"before"].toObject();const auto current=validatedLayout(defaultCategoryLayout(pid,cat,frame),categoryLayout(pid,cat,frame));if(current!=expected)throw Error("A posição foi alterada após a sincronização; nenhuma alteração aplicada.");layouts[cat]=change[undo?"before":"after"];}
    saveCategoryLayouts(pid,layouts,frame);
}
}
