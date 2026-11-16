#include "server.hpp"
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>
#include <memory>

static void initJpResources(){Q_INIT_RESOURCE(resources);}

namespace jp {
static QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
Server::Server(Store &s,QObject *p):QObject(p),store_(s),token_(randomKey()) {
    initJpResources();
    connect(&tcp_,&QTcpServer::newConnection,this,[this]{
        while(tcp_.hasPendingConnections()){
            auto *s=tcp_.nextPendingConnection();
            if(clients_.size()>=128){s->abort();s->deleteLater();continue;}
            clients_.insert(s,{});
            connect(s,&QTcpSocket::readyRead,this,[this,s]{read(s);});
            connect(s,&QTcpSocket::disconnected,this,[this,s]{clients_.remove(s);s->deleteLater();});
            QTimer::singleShot(10000,s,[this,s]{if(clients_.contains(s)&&clients_[s].stream.isEmpty()&&!clients_[s].busy)s->disconnectFromHost();});
        }
    });
    connect(&store_,&Store::changed,this,&Server::broadcast);
    heartbeat_.setInterval(15000);
    connect(&heartbeat_,&QTimer::timeout,this,[this]{for(auto *socket:clients_.keys())if(clients_.contains(socket)&&!clients_[socket].stream.isEmpty()){
        if(socket->bytesToWrite()>1024*1024)socket->abort();else socket->write(": heartbeat\n\n");
    }});
}
void Server::start(){
    quint16 port=store_.preferredPort();
    if(!tcp_.listen(QHostAddress::LocalHost,port))throw Error(QString("A porta local %1 está ocupada. Feche a outra instância ou ajuste 'port' em library.json com OBS fechado.").arg(port));
    store_.port(tcp_.serverPort());heartbeat_.start();
}
void Server::stop(){heartbeat_.stop();tcp_.close();const auto sockets=clients_.keys();for(auto *s:sockets)s->abort();clients_.clear();}
QString Server::baseUrl()const{return "http://127.0.0.1:"+QString::number(tcp_.serverPort());}
QString Server::overlayUrl(const QString &id)const{return baseUrl()+"/overlay/"+store_.project(id)["key"].toString();}
void Server::reply(QTcpSocket *s,int status,const QByteArray &body,const QByteArray &type,const QByteArray &extra){
    if(!clients_.contains(s))return;clients_[s].busy=true;
    s->write("HTTP/1.1 "+QByteArray::number(status)+" Response\r\nContent-Type: "+type+"\r\nContent-Length: "+QByteArray::number(body.size())+
        "\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\n"+
        "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; media-src 'self' blob:; connect-src 'self'; frame-src 'self'; frame-ancestors 'self'; object-src 'none'; base-uri 'none'\r\n"+extra+"\r\n"+body);
    s->disconnectFromHost();
}
void Server::read(QTcpSocket *s){
    auto it=clients_.find(s);if(it==clients_.end()||it->busy||!it->stream.isEmpty())return;
    it->input+=s->readAll();
    if(it->input.size()>768*1024){reply(s,413,"{}");return;}
    const auto end=it->input.indexOf("\r\n\r\n");if(end<0){if(it->input.size()>16384)reply(s,431,"{}");return;}
    if(end>16384){reply(s,431,"{}");return;}
    const auto lines=it->input.left(end).split('\n');const auto first=lines[0].trimmed().split(' ');
    if(first.size()!=3||first[2]!="HTTP/1.1"){reply(s,400,"{}");return;}
    QHash<QByteArray,QByteArray> h;
    for(int i=1;i<lines.size();++i){const auto line=lines[i].trimmed();const auto colon=line.indexOf(':');if(colon<1){reply(s,400,"{}");return;}
        const auto key=line.left(colon).toLower();if(h.contains(key)){reply(s,400,"{}");return;}h.insert(key,line.mid(colon+1).trimmed());}
    if(h["host"]!=("127.0.0.1:"+QByteArray::number(tcp_.serverPort()))||h.contains("transfer-encoding")){reply(s,403,"{}");return;}
    if(h.contains("origin")&&h["origin"]!=baseUrl().toLatin1()){reply(s,403,"{}");return;}
    if(h["sec-fetch-site"]=="cross-site"){reply(s,403,"{}");return;}
    bool ok=true;const qint64 size=h.contains("content-length")?h["content-length"].toLongLong(&ok):0;
    if(!ok||size<0||size>700*1024){reply(s,413,"{}");return;}
    if(it->input.size()<end+4+size)return;
    const auto body=it->input.mid(end+4,size);it->input.clear();
    const QUrl url=QUrl::fromEncoded(first[1],QUrl::StrictMode);
    if(!url.isValid()||!url.isRelative()||!first[1].startsWith('/')){reply(s,400,"{}");return;}
    try{route(s,first[0],url,h,body);}catch(const std::exception &e){reply(s,400,json({{"error",QString::fromUtf8(e.what())}}));}
}
void Server::file(QTcpSocket *s,const QString &path,const QByteArray &mime,const QByteArray &range){
    auto f=std::make_shared<QFile>(path);if(!f->open(QIODevice::ReadOnly))throw Error("Arquivo não encontrado.");
    const qint64 total=f->size();qint64 begin=0,end=total-1;int status=200;
    if(!range.isEmpty()){
        const auto match=QRegularExpression("^bytes=(\\d*)-(\\d*)$").match(QString::fromLatin1(range));
        bool valid=match.hasMatch()&&!(match.captured(1).isEmpty()&&match.captured(2).isEmpty());
        bool ok=true;
        if(valid){if(match.captured(1).isEmpty()){const auto n=match.captured(2).toLongLong(&ok);valid=ok&&n>0;begin=qMax(qint64(0),total-n);}else{
            begin=match.captured(1).toLongLong(&ok);valid=ok;if(!match.captured(2).isEmpty()){end=match.captured(2).toLongLong(&ok);valid=valid&&ok;}}
            end=qMin(end,total-1);valid=valid&&begin<=end&&begin>=0&&begin<total;}
        if(!valid){reply(s,416,"{}","application/json","Content-Range: bytes */"+QByteArray::number(total)+"\r\n");return;}status=206;
    }
    clients_[s].busy=true;auto left=std::make_shared<qint64>(end-begin+1);f->seek(begin);
    QByteArray header="HTTP/1.1 "+QByteArray::number(status)+" Response\r\nContent-Type: "+mime+"\r\nContent-Length: "+QByteArray::number(*left)+
        "\r\nAccept-Ranges: bytes\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nCache-Control: private, max-age=3600\r\n";
    if(status==206)header+="Content-Range: bytes "+QByteArray::number(begin)+"-"+QByteArray::number(end)+"/"+QByteArray::number(total)+"\r\n";
    s->write(header+"\r\n");
    auto pump=[s,f,left]{while(*left>0&&s->bytesToWrite()<256*1024){const auto b=f->read(qMin(*left,qint64(64*1024)));if(b.isEmpty()){s->abort();return;}s->write(b);*left-=b.size();}if(*left==0)s->disconnectFromHost();};
    connect(s,&QTcpSocket::bytesWritten,s,[pump](qint64){pump();});pump();
}
void Server::route(QTcpSocket *s,const QByteArray &method,const QUrl &url,const QHash<QByteArray,QByteArray> &h,const QByteArray &body){
    const auto path=url.path();const auto parts=path.split('/',Qt::SkipEmptyParts);
    const bool auth=h.value("authorization")==("Bearer "+token_).toLatin1();
    if(path.startsWith("/api/")){
        if(!auth){reply(s,401,"{\"error\":\"Sessão expirada. Reabra o painel no OBS.\"}");return;}
        if(method=="GET"&&path=="/api/state"){auto state=store_.snapshot();state["baseUrl"]=baseUrl();reply(s,200,json(state));return;}
        if(method!="POST"||path!="/api/command"){reply(s,405,"{}");return;}
        if(!h.value("content-type").startsWith("application/json"))throw Error("Envie JSON.");
        const auto d=QJsonDocument::fromJson(body);if(!d.isObject())throw Error("JSON inválido.");reply(s,200,json(command(d.object())));return;
    }
    if(method!="GET"){reply(s,405,"{}");return;}
    if(parts.size()==2&&parts[0]=="events"){
        const auto key=parts[1];store_.projectByKey(key);clients_[s].stream=key;
        s->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\nConnection: keep-alive\r\nX-Content-Type-Options: nosniff\r\n\r\n");
        s->write("data: "+json(store_.liveState(key))+"\n\n");return;
    }
    if(path=="/control-events"){
        // EventSource cannot set Authorization: panel uses fetch streaming instead.
        if(!auth){reply(s,401,"{}");return;}clients_[s].stream="control";clients_[s].panel=true;
        s->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-store\r\nConnection: keep-alive\r\n\r\ndata: {\"changed\":true}\n\n");return;
    }
    if(parts.size()==2&&parts[0]=="state"){reply(s,200,json(store_.liveState(parts[1])));return;}
    if(parts.size()==3&&(parts[0]=="media"||parts[0]=="thumb")){
        const auto p=store_.projectByKey(parts[1]);for(const auto &v:p["media"].toArray()){const auto m=v.toObject();if(m["id"].toString()!=parts[2])continue;
            const bool thumb=parts[0]=="thumb";file(s,store_.mediaPath(m,thumb),thumb?QByteArray("image/png"):m["mime"].toString().toLatin1(),h.value("range"));return;}throw Error("Mídia não encontrada.");
    }
    QString resource;
    if(path=="/panel"||path=="/")resource=":/jp/web/panel.html";
    else if(parts.size()==2&&parts[0]=="overlay"){store_.projectByKey(parts[1]);resource=":/jp/web/overlay.html";}
    else if(QStringList{"/panel.css","/panel.js","/overlay.css","/overlay.js"}.contains(path))resource=":/jp/web"+path;
    else if(path=="/branding.png")resource=":/jp/assets/branding/jpfachina-sync-original.png";
    else{reply(s,404,"{}");return;}
    QFile f(resource);if(!f.open(QIODevice::ReadOnly))throw Error("Recurso do painel indisponível.");
    QByteArray type="text/html; charset=utf-8";if(resource.endsWith(".css"))type="text/css";if(resource.endsWith(".js"))type="text/javascript";if(resource.endsWith(".png"))type="image/png";
    reply(s,200,f.readAll(),type);
}
QJsonObject Server::command(const QJsonObject &in){
    const auto action=in["action"].toString(),pid=in["projectId"].toString(),id=in["mediaId"].toString();
    if(action=="project.create")return {{"id",store_.createProject(in["name"].toString())}};
    if(action=="project.rename")store_.renameProject(pid,in["name"].toString());
    else if(action=="project.select")store_.selectProject(pid);
    else if(action=="project.delete"){if(!in["confirm"].toBool())throw Error("Confirme a exclusão.");store_.removeProject(pid);}
    else if(action=="category.save")store_.categories(pid,in["categories"].toArray());
    else if(action=="media.update")store_.updateMedia(pid,id,in["patch"].toObject());
    else if(action=="media.on")store_.setLive(pid,id,true);
    else if(action=="media.off")store_.setLive(pid,id,false);
    else if(action=="media.delete"){if(!in["confirm"].toBool())throw Error("Confirme a exclusão.");store_.removeMedia(pid,id);}
    else if(action=="live.clear"){if(!in["confirm"].toBool())throw Error("Confirme a limpeza.");store_.clearLive(pid);}
    else if(action=="media.thumbnail"){const auto data=in["png"].toString();if(!data.startsWith("data:image/png;base64,"))throw Error("Miniatura inválida.");store_.saveThumbnail(pid,id,QByteArray::fromBase64(data.mid(22).toLatin1()));}
    else if(action=="media.download"){store_.project(pid);emit downloadRequested(pid,in["url"].toString(),in["category"].toString());}
    else if(action=="native.import"||action=="native.source"||action=="native.folder"||action=="native.about"){
        if(action!="native.about")store_.project(pid);QTimer::singleShot(0,this,[this,action,pid,in]{emit nativeAction(action,pid,in["category"].toString());});
    }else throw Error("Comando desconhecido.");return {{"ok",true}};
}
void Server::broadcast(){
    for(auto *socket:clients_.keys()){if(!clients_.contains(socket)||clients_[socket].stream.isEmpty())continue;
        const auto client=clients_[socket];
        if(socket->bytesToWrite()>1024*1024){socket->disconnectFromHost();continue;}
        if(client.panel)socket->write("data: {\"changed\":true}\n\n");
        else{QJsonObject state;try{state=store_.liveState(client.stream);}catch(...){state={{"width",1920},{"height",1080},{"items",QJsonArray{}},{"deleted",true}};}socket->write("data: "+json(state)+"\n\n");}
    }
}
void Server::notice(const QString &message,bool error){
    if(error)qWarning().noquote()<<"[JP Fachina Graphics]"<<message;else qInfo().noquote()<<"[JP Fachina Graphics]"<<message;
    for(auto *socket:clients_.keys())if(clients_.contains(socket)&&clients_[socket].panel){
        if(socket->bytesToWrite()>1024*1024)socket->disconnectFromHost();else socket->write("data: "+json({{"message",message},{"error",error}})+"\n\n");
    }
}
}
