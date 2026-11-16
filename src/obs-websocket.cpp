#include "obs-websocket.hpp"
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace jp {
static QByteArray protect(const QByteArray &bytes,bool decrypt){
#ifdef Q_OS_WIN
    DATA_BLOB input{DWORD(bytes.size()),reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))},output{};
    const bool ok=decrypt?CryptUnprotectData(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output):CryptProtectData(&input,L"JPFachina OBS",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output);
    if(!ok)throw Error("Não foi possível acessar o armazenamento seguro de credenciais.");
    QByteArray result(reinterpret_cast<const char*>(output.pbData),int(output.cbData));SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return result;
#else
    Q_UNUSED(bytes);Q_UNUSED(decrypt);throw Error("Armazenamento seguro disponível somente no Windows.");
#endif
}
static QString secret(Store &store,const QString &key,const QString &password={}){
    const auto path=store.root()+"/obs-credentials.json";QJsonObject values;QFile f(path);if(f.open(QIODevice::ReadOnly)){if(f.size()>1024*1024)throw Error("Arquivo de credenciais inválido.");values=QJsonDocument::fromJson(f.readAll()).object();f.close();}
    if(!password.isEmpty()){values[key]=QString::fromLatin1(protect(password.toUtf8(),false).toBase64());QSaveFile out(path);const auto bytes=QJsonDocument(values).toJson();if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit())throw Error("Falha ao guardar a senha protegida.");return password;}
    const auto encrypted=QByteArray::fromBase64(values[key].toString().toLatin1());return encrypted.isEmpty()?QString():QString::fromUtf8(protect(encrypted,true));
}
QString ObsWebSocket::authentication(const QString &password,const QString &salt,const QString &challenge){const auto first=QCryptographicHash::hash((password+salt).toUtf8(),QCryptographicHash::Sha256).toBase64();return QString::fromLatin1(QCryptographicHash::hash(first+challenge.toUtf8(),QCryptographicHash::Sha256).toBase64());}
ObsWebSocket::ObsWebSocket(QObject *parent):QObject(parent){
    clock_.start();reconnect_.setSingleShot(true);handshake_.setSingleShot(true);handshake_.setInterval(7000);timeouts_.setInterval(250);timeouts_.start();socket_.setMaxAllowedIncomingMessageSize(4*1024*1024);
    connect(&reconnect_,&QTimer::timeout,this,&ObsWebSocket::connectSocket);connect(&handshake_,&QTimer::timeout,this,[this]{emit message("Tempo esgotado na conexão OBS WebSocket.");socket_.abort();lost();});
    connect(&socket_,&QWebSocket::textMessageReceived,this,&ObsWebSocket::receive);connect(&socket_,&QWebSocket::disconnected,this,[this]{if(int(socket_.closeCode())==4009){wanted_=false;emit message("Autenticação OBS recusada. Confira a senha em Link OBS.");}lost();});connect(&socket_,&QWebSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){if(wanted_){emit message("Conexão OBS WebSocket indisponível.");socket_.abort();lost();}});
    connect(&timeouts_,&QTimer::timeout,this,[this]{QStringList expired;for(auto it=pending_.begin();it!=pending_.end();++it)if(it.value().deadline<clock_.elapsed())expired<<it.key();if(expired.isEmpty())return;for(const auto &id:expired){auto callback=pending_.take(id).reply;if(callback)callback({},"OBS não respondeu no prazo.");}socket_.abort();lost();});
}
void ObsWebSocket::open(const QJsonObject &configuration,const QString &password){close();config_=configuration;password_=password;attempt_=0;wanted_=true;connectSocket();}
void ObsWebSocket::close(){wanted_=false;reconnect_.stop();handshake_.stop();socket_.abort();lost();password_.clear();}
void ObsWebSocket::connectSocket(){if(!wanted_||socket_.state()!=QAbstractSocket::UnconnectedState)return;QUrl url;url.setScheme(config_["tls"].toBool()?"wss":"ws");url.setHost(config_["host"].toString());url.setPort(config_["port"].toInt(4455));handshake_.start();socket_.open(url);}
void ObsWebSocket::lost(){const bool connected=ready_;ready_=false;handshake_.stop();const auto pending=std::move(pending_);pending_.clear();for(const auto &item:pending)if(item.reply)item.reply({},"Conexão OBS interrompida.");if(connected)emit disconnected();if(wanted_&&!reconnect_.isActive()){const int delay=qMin(30000,1000*(1<<qMin(attempt_++,5)));reconnect_.start(delay);}}
void ObsWebSocket::receive(const QString &text){
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(text.toUtf8(),&error);if(error.error!=QJsonParseError::NoError||!doc.isObject()){socket_.abort();return;}const auto message=doc.object(),data=message["d"].toObject();const int op=message["op"].toInt(-1);
    if(op==0){QJsonObject identify{{"rpcVersion",1},{"eventSubscriptions",2047}};if(data.contains("authentication")){const auto auth=data["authentication"].toObject();identify["authentication"]=authentication(password_,auth["salt"].toString(),auth["challenge"].toString());}socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op",1},{"d",identify}}).toJson(QJsonDocument::Compact)));}
    else if(op==2){handshake_.stop();ready_=true;attempt_=0;emit identified();}
    else if(op==5&&ready_)emit event(data["eventType"].toString(),data["eventData"].toObject());
    else if(op==7&&ready_){const auto id=data["requestId"].toString();if(!pending_.contains(id))return;const auto callback=pending_.take(id).reply;const auto status=data["requestStatus"].toObject();if(callback)callback(data["responseData"].toObject(),status["result"].toBool()?QString():QString("OBS recusou a operação (%1).").arg(status["code"].toInt()));}
}
void ObsWebSocket::request(const QString &type,const QJsonObject &data,Reply reply){if(!ready_){reply({},"OBS WebSocket desconectado.");return;}if(pending_.size()>=128){reply({},"Aguarde as operações OBS em andamento.");return;}const auto id=QString::number(++sequence_);pending_.insert(id,{std::move(reply),clock_.elapsed()+7000});const QJsonObject request{{"requestType",type},{"requestId",id},{"requestData",data}};socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op",6},{"d",request}}).toJson(QJsonDocument::Compact)));}

struct ObsConnections::Endpoint {
    std::unique_ptr<ObsWebSocket> client;
    QString key;
    QJsonObject config;
    QJsonArray scenes;
    QMap<QString,QJsonObject> sources,sent;
    QSet<QString> updating;
    QSize frame{1920,1080};
    bool loading=false,polling=false;
    QMap<QString,QPair<qint64,qint64>> progress;
    QElapsedTimer clock;
};
QString ObsConnections::key(const QJsonObject &config){const QJsonObject target{{"mode",config["mode"].toString("native")},{"host",config["host"].toString("127.0.0.1")},{"port",config["port"].toInt(4455)},{"tls",config["tls"].toBool()}};return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(target).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
bool ObsConnections::remote()const{return c_.project()["obsLink"].toObject()["mode"].toString()=="websocket";}
ObsConnections::ObsConnections(Controller &c,QObject *parent):QObject(parent),c_(c){
    nativeReconnect_=c_.reconnect;nativePreflight_=c_.preflightTake;nativeRefresh_=c_.scenesRefresh;nativeScenes_=c_.scenes;nativeAction_=c_.sceneAction;nativeVideoSize_=c_.videoSize;
    c_.connectionRequested=[this](const QJsonObject &settings,const QString &password){configure(settings,password);};
    c_.reconnect=[this]{if(remote())connectActive();else if(nativeReconnect_)nativeReconnect_();};
    c_.scenesRefresh=[this]{if(remote()){auto e=endpoint();if(e&&e->client->ready())refreshScenes(e);else throw Error("Conecte ao OBS WebSocket.");}else if(nativeRefresh_)nativeRefresh_();};
    c_.scenes=[this]{if(remote()){auto e=endpoint();return e?e->scenes:QJsonArray{};}return nativeScenes_?nativeScenes_():QJsonArray{};};
    c_.videoSize=[this]{if(remote()){auto e=endpoint();return e?e->frame:QSize(1920,1080);}return nativeVideoSize_?nativeVideoSize_():QSize(c_.project()["width"].toInt(1920),c_.project()["height"].toInt(1080));};
    c_.sceneAction=[this](const QString &scene,const QString &action,bool enabled,const QString &category){if(remote())sceneAction(scene,action,enabled,category);else if(nativeAction_)nativeAction_(scene,action,enabled,category);else throw Error("Abra dentro do OBS ou conecte por WebSocket.");};
    c_.preflightTake=[this]{if(!remote()){if(nativePreflight_)nativePreflight_();return;}auto e=endpoint();if(!e||!e->client->ready())throw Error("OBS WebSocket desconectado.");const auto m=c_.selected();const auto cat=panelCategory(m["category"].toString(),m["mime"].toString());bool associated=false;for(const auto &v:c_.project()["obsBindings"].toArray()){const auto b=v.toObject(),s=e->sources.value(b["sourceUuid"].toString());if(b["category"].toString()==cat&&!b["missing"].toBool()&&s["project"].toString()==c_.projectId())associated=true;}if(!associated)throw Error("Fonte da categoria inexistente ou não associada. Atualize Link OBS.");};
    refresh_.setSingleShot(true);refresh_.setInterval(40);connect(&refresh_,&QTimer::timeout,this,&ObsConnections::refreshOutputs);poll_.setInterval(750);connect(&poll_,&QTimer::timeout,this,[this]{for(const auto &e:endpoints_)if(e->client->ready())poll(e);});poll_.start();
    connect(&c_.store,&Store::changed,this,[this]{refresh_.start();projectChanged();});connect(&c_,&Controller::changed,this,[this]{projectChanged();if(!c_.linked&&remote()){auto e=endpoint();if(e&&e->client->ready())e->client->close();}});QTimer::singleShot(0,this,[this]{projectChanged();});
}
ObsConnections::~ObsConnections(){poll_.stop();refresh_.stop();for(const auto &e:endpoints_){QObject::disconnect(e->client.get(),nullptr,this,nullptr);e->client->close();e->client.reset();}}
std::shared_ptr<ObsConnections::Endpoint> ObsConnections::endpoint(bool create){
    const auto config=c_.project()["obsLink"].toObject();const auto k=key(config);if(endpoints_.contains(k))return endpoints_[k];if(!create)return {};
    auto e=std::make_shared<Endpoint>();e->clock.start();e->key=k;e->config=config;e->client=std::make_unique<ObsWebSocket>();endpoints_[k]=e;
    connect(e->client.get(),&ObsWebSocket::identified,this,[this,e]{e->sent.clear();e->updating.clear();e->progress.clear();e->loading=false;e->polling=false;if(remote()&&key(c_.project()["obsLink"].toObject())==e->key)c_.setObsState(true,false,false,"OBS WebSocket conectado · consultando estado real");refreshScenes(e);e->client->request("GetVideoSettings",{},[e](QJsonObject data,QString error){if(error.isEmpty())e->frame={data["baseWidth"].toInt(1920),data["baseHeight"].toInt(1080)};});});
    connect(e->client.get(),&ObsWebSocket::disconnected,this,[this,e]{e->sources.clear();e->updating.clear();e->loading=false;e->polling=false;c_.confirmProvider(e->key,{});if(remote()&&key(c_.project()["obsLink"].toObject())==e->key)c_.setObsState(false,false,false,"OBS WebSocket desconectado · reconexão controlada");});
    connect(e->client.get(),&ObsWebSocket::message,this,[this,e](const QString &text){if(remote()&&key(c_.project()["obsLink"].toObject())==e->key)c_.setObsState(false,false,false,text);});
    connect(e->client.get(),&ObsWebSocket::event,this,[this,e](const QString &type,const QJsonObject &){if(type.contains("Scene")||type.contains("InputCreated")||type.contains("InputRemoved")||type.contains("InputName"))QTimer::singleShot(80,this,[this,e]{refreshScenes(e);});});return e;
}
void ObsConnections::configure(const QJsonObject &configuration,const QString &password){
    const auto old=c_.project()["obsLink"].toObject();if(key(old)!=key(configuration)||old["mediaRoot"].toString()!=configuration["mediaRoot"].toString()){for(const auto &v:c_.project()["media"].toArray())if(v.toObject()["live"].isObject())throw Error("Retire os conteúdos deste projeto antes de trocar o destino OBS.");}
    c_.store.obsLink(c_.projectId(),configuration);activeProject_=c_.projectId();c_.linked=true;
    if(remote()){c_.setObsState(false,false,false,"Conectando ao OBS WebSocket…");connectActive(password);}else{c_.obsScenes={};if(nativeReconnect_)nativeReconnect_();else c_.setObsState(false,false,false,"Integração nativa disponível somente dentro do OBS.");}
}
void ObsConnections::connectActive(const QString &password){auto e=endpoint(true);const auto credential=secret(c_.store,e->key,password);if(e->client->ready()&&password.isEmpty()){refreshScenes(e);c_.setObsState(true,false,false,"OBS WebSocket conectado");return;}c_.setObsState(false,false,false,"Conectando ao OBS WebSocket…");e->client->open(e->config,credential);}
void ObsConnections::projectChanged(){const auto pid=c_.projectId();if(pid==activeProject_)return;activeProject_=pid;c_.obsScenes={};emit c_.obsScenesChanged();c_.linked=true;if(remote()){try{connectActive();}catch(const Error &e){c_.notify(QString::fromUtf8(e.what()),true);}}else if(nativeReconnect_)nativeReconnect_();else c_.setObsState(false,false,false,"OBS local indisponível · configure Link OBS.");}
void ObsConnections::refreshScenes(const std::shared_ptr<Endpoint> &e){
    if(!e->client->ready()||e->loading)return;e->loading=true;
    e->client->request("GetSceneList",{},[this,e](QJsonObject result,QString error){
        if(!error.isEmpty()){e->loading=false;return;}const auto sceneRows=result["scenes"].toArray();auto rows=std::make_shared<QJsonArray>();auto sources=std::make_shared<QMap<QString,QJsonObject>>();auto remaining=std::make_shared<int>(int(sceneRows.size()));
        auto finished=[this,e,rows,sources]{e->loading=false;e->scenes=*rows;e->sources=*sources;
            for(const auto &v:c_.store.snapshot()["projects"].toArray()){const auto p=v.toObject();if(key(p["obsLink"].toObject())!=e->key)continue;auto bindings=p["obsBindings"].toArray();for(int i=0;i<bindings.size();++i){auto b=bindings[i].toObject();bool found=false;for(const auto &r:e->scenes){const auto scene=r.toObject();if(scene["uuid"]!=b["sceneUuid"])continue;for(const auto &s:scene["sources"].toArray()){const auto source=s.toObject();if(source["uuid"]==b["sourceUuid"]&&source["project"]==p["id"]){found=true;b["sourceName"]=source["name"];b["sceneName"]=scene["name"];}}}b["missing"]=!found;bindings[i]=b;}if(bindings!=p["obsBindings"].toArray())c_.store.obsBindings(p["id"].toString(),bindings);}
            if(remote()&&key(c_.project()["obsLink"].toObject())==e->key){c_.obsScenes=e->scenes;emit c_.obsScenesChanged();}refresh_.start();poll(e);
        };
        if(sceneRows.isEmpty()){finished();return;}
        for(const auto &v:sceneRows){const auto scene=v.toObject();e->client->request("GetSceneItemList",{{"sceneUuid",scene["sceneUuid"]}},[this,e,scene,rows,sources,remaining,finished](QJsonObject data,QString failure){
            auto row=std::make_shared<QJsonObject>(QJsonObject{{"name",scene["sceneName"]},{"uuid",scene["sceneUuid"]}});auto own=std::make_shared<QJsonArray>();QJsonArray candidates;
            if(failure.isEmpty())for(const auto &item:data["sceneItems"].toArray())if(item.toObject()["inputKind"].toString()=="jp_fachina_composition"||item.toObject()["inputKind"].toString().startsWith("jp_fachina_composition"))candidates.append(item);
            auto pending=std::make_shared<int>(int(candidates.size()));auto endRow=[rows,row,own,remaining,finished]{(*row)["sources"]=*own;rows->append(*row);if(--*remaining==0)finished();};if(candidates.isEmpty()){endRow();return;}
            for(const auto &v:candidates){const auto item=v.toObject();e->client->request("GetInputSettings",{{"inputUuid",item["sourceUuid"]}},[item,row,own,sources,pending,endRow](QJsonObject response,QString error){if(error.isEmpty()){const auto settings=response["inputSettings"].toObject();const auto pid=settings["jp_fachina_project"].toString();if(!pid.isEmpty()){QJsonObject source{{"name",item["sourceName"]},{"uuid",item["sourceUuid"]},{"project",pid},{"category",settings["jp_fachina_category"]},{"visible",item["sceneItemEnabled"]},{"itemId",item["sceneItemId"]},{"settings",settings}};own->append(source);(*sources)[source["uuid"].toString()]=source;}}if(--*pending==0)endRow();});}
        });}
    });
}
void ObsConnections::refreshOutputs(){
    for(const auto &e:endpoints_){if(!e->client->ready())continue;
        for(const auto &v:c_.store.snapshot()["projects"].toArray()){const auto project=v.toObject();if(key(project["obsLink"].toObject())!=e->key)continue;QSet<QString> seen;
            for(const auto &bindingValue:project["obsBindings"].toArray()){const auto binding=bindingValue.toObject();const auto uuid=binding["sourceUuid"].toString();if(seen.contains(uuid)||binding["missing"].toBool()||!e->sources.contains(uuid))continue;seen.insert(uuid);auto p=project;const auto source=e->sources.value(uuid);const auto category=source["category"].toString();if(!category.isEmpty()){QJsonArray media;for(const auto &item:p["media"].toArray())if(panelCategory(item.toObject()["category"].toString(),item.toObject()["mime"].toString())==category)media.append(item);p["media"]=media;}
                QJsonObject state;try{state=compositionState(c_.store,p);const auto remoteRoot=p["obsLink"].toObject()["mediaRoot"].toString();if(!remoteRoot.isEmpty()){auto items=state["items"].toArray();for(int i=0;i<items.size();++i){auto item=items[i].toObject();const auto path=item["path"].toString();const auto relative=QDir(QFileInfo(c_.store.root()).canonicalFilePath()).relativeFilePath(QFileInfo(path).canonicalFilePath());if(relative.startsWith("../"))throw Error("Mídia fora da biblioteca.");item["path"]=QDir::fromNativeSeparators(remoteRoot)+"/"+relative;items[i]=item;}state["items"]=items;}}catch(const Error &error){c_.notify(QString::fromUtf8(error.what()),true);continue;}if(e->sent.value(uuid)==state||e->updating.contains(uuid))continue;e->updating.insert(uuid);
                e->client->request("GetInputSettings",{{"inputUuid",uuid}},[this,e,uuid,state,p](QJsonObject data,QString error){const auto settings=data["inputSettings"].toObject();if(!error.isEmpty()||settings["jp_fachina_project"]!=p["id"]){e->updating.remove(uuid);if(error.isEmpty())c_.notify("Fonte deixou de pertencer ao projeto. Nenhuma alteração aplicada.",true);refreshScenes(e);return;}
                    e->client->request("SetInputSettings",{{"inputUuid",uuid},{"inputSettings",QJsonObject{{"state",QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact))}}},{"overlay",true}},[this,e,uuid,state](QJsonObject,QString error){e->updating.remove(uuid);if(error.isEmpty()){e->sent[uuid]=state;refresh_.start();}else c_.notify(error,true);});
                });
            }
        }
    }
}
void ObsConnections::poll(const std::shared_ptr<Endpoint> &e){
    if(e->polling||!e->client->ready())return;e->polling=true;auto ids=std::make_shared<QMap<QString,QSet<QString>>>();auto generations=std::make_shared<QMap<QString,QMap<QString,int>>>();auto remaining=std::make_shared<int>(e->sources.size());
    auto done=[this,e,ids,generations]{e->polling=false;c_.confirmProvider(e->key,*ids,*generations);};if(e->sources.isEmpty()){done();return;}
    for(auto it=e->sources.begin();it!=e->sources.end();++it){const auto uuid=it.key();e->client->request("GetInputSettings",{{"inputUuid",uuid}},[this,e,uuid,ids,generations,remaining,done](QJsonObject data,QString error){
        if(error.isEmpty()){const auto settings=data["inputSettings"].toObject();const auto pid=settings["jp_fachina_project"].toString();const auto actual=QJsonDocument::fromJson(settings["jp_fachina_observed"].toString().toUtf8()).object();const auto sequence=qint64(actual["sequence"].toDouble());const bool seen=e->progress.contains(uuid);auto progress=e->progress.value(uuid);const bool fresh=seen&&sequence>0&&(sequence!=progress.first||e->clock.elapsed()-progress.second<3000);if(!seen||sequence!=progress.first)e->progress[uuid]={sequence,e->clock.elapsed()};if(fresh&&actual["program"].toBool())for(const auto &v:actual["items"].toArray()){const auto item=v.toObject();(*ids)[pid].insert(item["id"].toString());(*generations)[pid][item["id"].toString()]=item["generation"].toInt();}
            try{const auto p=c_.store.project(pid);for(const auto &ended:actual["ended"].toArray())for(const auto &v:p["media"].toArray()){const auto m=v.toObject(),end=ended.toObject();if(m["id"]==end["id"]&&m["generation"]==end["generation"]&&m["live"].isObject()&&m["live"].toObject()["endBehavior"].toString("hold")=="out")c_.store.setLive(pid,m["id"].toString(),false);}}catch(const Error &){}
        }if(--*remaining==0)done();
    });}
}
void ObsConnections::associate(const std::shared_ptr<Endpoint> &e,const QString &scene,const QString &category){
    const auto project=c_.project();const auto pid=c_.projectId();QJsonObject row;for(const auto &v:e->scenes)if(v.toObject()["uuid"].toString()==scene)row=v.toObject();if(row.isEmpty())throw Error("Cena inexistente; atualize Link OBS.");
    for(const auto &v:row["sources"].toArray()){const auto s=v.toObject();if(s["project"]==pid&&s["visible"].toBool()&&s["category"].toString()!=category&&(s["category"].toString().isEmpty()||category.isEmpty()))throw Error("Oculte o vínculo que já cobre essa categoria antes de criar outro, para evitar exibição duplicada.");}
    const auto name="JP Graphics · "+pid+(category.isEmpty()?QString():" · "+category);QJsonObject existing;for(const auto &s:e->sources)if(s["project"]==pid&&s["category"].toString()==category){existing=s;break;}
    auto bound=[this,e,pid,row,category](QJsonObject source){auto bindings=c_.store.project(pid)["obsBindings"].toArray();for(const auto &cat:category.isEmpty()?panelCategories():QStringList{category}){QJsonObject b{{"sceneUuid",row["uuid"]},{"sceneName",row["name"]},{"sourceUuid",source["uuid"]},{"sourceName",source["name"]},{"category",cat},{"missing",false}};bool replaced=false;for(int i=0;i<bindings.size();++i)if(bindings[i].toObject()["sceneUuid"]==b["sceneUuid"]&&bindings[i].toObject()["category"]==b["category"]){bindings[i]=b;replaced=true;break;}if(!replaced)bindings.append(b);}c_.store.obsBindings(pid,bindings);refreshScenes(e);};
    if(!existing.isEmpty()){for(const auto &v:row["sources"].toArray())if(v.toObject()["uuid"]==existing["uuid"]){bound(existing);return;}e->client->request("CreateSceneItem",{{"sceneUuid",row["uuid"]},{"sourceUuid",existing["uuid"]},{"sceneItemEnabled",true}},[this,bound,existing](QJsonObject,QString error){if(error.isEmpty())bound(existing);else c_.notify(error,true);});return;}
    auto p=project;if(!category.isEmpty()){QJsonArray media;for(const auto &v:p["media"].toArray())if(panelCategory(v.toObject()["category"].toString(),v.toObject()["mime"].toString())==category)media.append(v);p["media"]=media;}
    const auto state=QString::fromUtf8(QJsonDocument(compositionState(c_.store,p)).toJson(QJsonDocument::Compact));QJsonObject settings{{"jp_fachina_project",pid},{"jp_fachina_category",category},{"jp_fachina_external",true},{"state",state}};
    e->client->request("CreateInput",{{"sceneUuid",row["uuid"]},{"inputName",name},{"inputKind","jp_fachina_composition"},{"inputSettings",settings},{"sceneItemEnabled",true}},[this,e,bound,name,pid,category](QJsonObject data,QString error){if(!error.isEmpty()){c_.notify("Associação recusada. Instale o plugin 0.4 no OBS de destino e verifique a cena e o nome da fonte.",true);return;}bound({{"uuid",data["inputUuid"]},{"name",name},{"project",pid},{"category",category}});});
}
void ObsConnections::sceneAction(const QString &scene,const QString &action,bool enabled,const QString &category){
    auto e=endpoint();if(!e||!e->client->ready())throw Error("OBS WebSocket desconectado.");if(action=="associate"){associate(e,scene,category);return;}
    QJsonObject row;for(const auto &v:e->scenes)if(v.toObject()["uuid"].toString()==scene)row=v.toObject();
    if(enabled)for(const auto &v:row["sources"].toArray()){const auto source=v.toObject();if(source["project"].toString()==c_.projectId()&&source["visible"].toBool()&&source["category"].toString()!=category&&(source["category"].toString().isEmpty()||category.isEmpty()))throw Error("Oculte primeiro a composição sobreposta, para evitar exibição duplicada.");}
    bool found=false;const auto pid=c_.projectId();
    for(const auto &v:row["sources"].toArray()){const auto source=v.toObject();if(source["project"].toString()!=pid||source["category"].toString()!=category)continue;found=true;
        e->client->request("GetInputSettings",{{"inputUuid",source["uuid"]}},[this,e,source,scene,pid,enabled](QJsonObject data,QString error){if(!error.isEmpty()||data["inputSettings"].toObject()["jp_fachina_project"].toString()!=pid){c_.notify("Fonte inexistente ou fora do gerenciamento; vínculo preservado sem alterações.",true);refreshScenes(e);return;}
            e->client->request("SetSceneItemEnabled",{{"sceneUuid",scene},{"sceneItemId",source["itemId"]},{"sceneItemEnabled",enabled}},[this,e](QJsonObject,QString error){if(!error.isEmpty())c_.notify(error,true);refreshScenes(e);});
        });
    }
    if(!found)throw Error("Fonte gerenciada inexistente para esta categoria.");
}
}
