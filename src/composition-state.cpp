#include "store.hpp"
#include "title-image.hpp"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QDebug>
namespace jp {
QJsonObject compositionState(Store &store,const QJsonObject &project,const QString &selected,const QJsonObject &draft){
    QJsonArray items;const bool preview=!selected.isEmpty();
    for(const auto &v:project["media"].toArray()){auto m=v.toObject();if(preview&&m["id"].toString()!=selected)continue;if(!preview&&!m["live"].isObject())continue;
        auto layout=preview?draft:m["live"].toObject();if(preview){layout["transition"]="none";layout["duration"]=0;}
        QString path;
        if(m["mime"]=="application/x-jp-title"){
            const QJsonObject visual{{"width",layout.value("width")},{"height",layout.value("height")},{"text",layout.value("text")}};
            const auto bytes=QJsonDocument(visual).toJson(QJsonDocument::Compact);const auto hash=QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex();
            QDir().mkpath(store.root()+"/titles");path=store.root()+"/titles/"+QString::fromLatin1(hash)+".png";
            if(!QFileInfo::exists(path)){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||!titleImage(layout).save(&f,"PNG")||!f.commit())throw Error("Falha ao preparar texto da tarjeta.");}
        }else{try{path=store.mediaPath(m);}catch(const std::exception &e){qWarning().noquote()<<"[JP Fachina Graphics]"<<e.what();continue;}}
        items.append(QJsonObject{{"id",m["id"]},{"name",m["name"]},{"mime",m["mime"]},{"path",path},{"layout",layout},{"generation",preview?0:m["generation"].toInt()}});
    }
    return {{"width",project["width"].toInt(1920)},{"height",project["height"].toInt(1080)},{"items",items}};
}
}
