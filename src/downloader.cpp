#include "downloader.hpp"
#include <QHostInfo>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QDir>
#include <QTimer>
#include <QFileInfo>

namespace jp {
Downloader::Downloader(Store &s,QObject *p):QObject(p),store_(s){manager_.setProxy(QNetworkProxy::NoProxy);}
Downloader::~Downloader(){stopping_=true;for(int id:lookups_)QHostInfo::abortHostLookup(id);const auto copy=replies_;for(auto *r:copy){r->disconnect(this);r->abort();}}
void Downloader::resolve(const QUrl &url,std::function<void(QString)> callback){
    if(!url.isValid()||!(url.scheme()=="http"||url.scheme()=="https")||!url.userInfo().isEmpty()||url.host().isEmpty()||url.hasFragment()||
        (url.port()!=-1&&url.port()!=80&&url.port()!=443)){callback("Use URL HTTP(S) pública, sem senha, fragmento ou porta personalizada.");return;}
    // Both resolutions must be public. No proxy; Qt still performs its own DNS resolution.
    const int id=QHostInfo::lookupHost(url.host(),this,[this,callback](const QHostInfo &info){
        lookups_.remove(info.lookupId());
        if(info.error()!=QHostInfo::NoError||info.addresses().isEmpty()){callback("Não foi possível resolver o endereço.");return;}
        for(const auto &a:info.addresses())if(!publicAddress(a.toString())){callback("Endereços locais, privados ou reservados não são permitidos.");return;}
        callback({});
    });lookups_.insert(id);
}
void Downloader::fetch(const QString &pid,const QString &input,const QString &category){
    if(active_>=2){emit completed("Há dois downloads em andamento. Aguarde.",true);return;}
    const QUrl url(input,QUrl::StrictMode);++active_;
    resolve(url,[this,pid,url,category](const QString &error){
        if(!error.isEmpty()){--active_;emit completed(error,true);return;}
        // Pin approved DNS result by using a literal IP URL. For HTTPS retain peer verification and SNI.
        const int lookup=QHostInfo::lookupHost(url.host(),this,[this,pid,url,category](const QHostInfo &info){
            lookups_.remove(info.lookupId());
            if(info.error()!=QHostInfo::NoError||info.addresses().isEmpty()){--active_;emit completed("DNS indisponível.",true);return;}
            for(const auto &a:info.addresses())if(!publicAddress(a.toString())){--active_;emit completed("Destino de download bloqueado.",true);return;}
            auto pinned=url;pinned.setHost(info.addresses().first().toString());QNetworkRequest request(pinned);
            request.setRawHeader("Host",url.host(QUrl::FullyEncoded).toLatin1());request.setPeerVerifyName(url.host());
            // HTTP/2 derives :authority from the literal IP URL; use HTTP/1.1 to retain the approved Host.
            request.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
            request.setTransferTimeout(30000);request.setRawHeader("Accept-Encoding","identity");
            request.setRawHeader("Accept","image/png,image/jpeg,image/webp,image/gif,video/webm,video/mp4");
            auto temp=std::make_shared<QTemporaryFile>(store_.root()+"/download-XXXXXX");
            if(!temp->open()){--active_;emit completed("Não foi possível criar o arquivo de download.",true);return;}
            auto *r=manager_.get(request);replies_.insert(r);
            auto failure=std::make_shared<QString>();auto total=std::make_shared<qint64>(0);
            auto drain=[r,temp,failure,total]{
                if(!r->isOpen())return;
                const auto b=r->readAll();*total+=b.size();
                if(*total>256LL*1024*1024){*failure="Download excede 256 MB.";r->abort();return;}
                if(temp->write(b)!=b.size()){*failure="Falha ao gravar download.";r->abort();}
            };
            connect(r,&QNetworkReply::readyRead,this,drain);
            connect(r,&QNetworkReply::metaDataChanged,this,[r,failure]{
                const auto size=r->header(QNetworkRequest::ContentLengthHeader).toLongLong();
                const auto mime=r->header(QNetworkRequest::ContentTypeHeader).toString().section(';',0,0).trimmed().toLower();
                const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if(size>256LL*1024*1024){*failure="Download excede 256 MB.";r->abort();}
                if(status==200&&!mime.isEmpty()&&!QStringList{"image/png","image/jpeg","image/webp","image/gif","video/webm","video/mp4","application/octet-stream"}.contains(mime)){
                    *failure="O servidor retornou um tipo incompatível: "+mime;r->abort();}
            });
            QTimer::singleShot(120000,r,[r,failure]{if(!r->isFinished()){*failure="Download excedeu dois minutos.";r->abort();}});
            connect(r,&QNetworkReply::finished,this,[this,r,temp,failure,pid,url,category,drain]{
                drain();replies_.remove(r);--active_;r->deleteLater();if(stopping_)return;
                if(!failure->isEmpty()){emit completed(*failure,true);return;}
                const int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if(status>=300&&status<400){emit completed("URL redirecionada. Use a URL final direta do arquivo.",true);return;}
                if(r->error()!=QNetworkReply::NoError||status!=200){emit completed("Falha no download: "+r->errorString(),true);return;}
                temp->flush();
                try{store_.importFileAsync(pid,temp->fileName(),category,url.toString(),true);temp->setAutoRemove(false);emit completed("Download concluído; preparando cache local.",false);
                }catch(const std::exception &e){emit completed(QString::fromUtf8(e.what()),true);}
            });
        });lookups_.insert(lookup);
    });
}
}
