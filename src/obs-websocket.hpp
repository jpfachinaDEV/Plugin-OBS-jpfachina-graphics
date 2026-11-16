#pragma once
#include "controller.hpp"
#include <QWebSocket>
#include <QTimer>
#include <QHash>
#include <QElapsedTimer>
#include <memory>

namespace jp {
class ObsWebSocket : public QObject {
    Q_OBJECT
public:
    using Reply=std::function<void(QJsonObject,QString)>;
    explicit ObsWebSocket(QObject *parent=nullptr);
    void open(const QJsonObject &configuration,const QString &password);
    void close();
    bool ready()const{return ready_;}
    QJsonObject config()const{return config_;}
    void request(const QString &type,const QJsonObject &data,Reply reply);
    static QString authentication(const QString &password,const QString &salt,const QString &challenge);
signals:
    void identified();
    void disconnected();
    void event(const QString &type,const QJsonObject &data);
    void message(const QString &text);
private:
    struct Pending{Reply reply;qint64 deadline;};
    QWebSocket socket_;
    QTimer reconnect_,timeouts_,handshake_;
    QElapsedTimer clock_;
    QJsonObject config_;
    QString password_;
    QHash<QString,Pending> pending_;
    bool ready_=false,wanted_=false;
    int attempt_=0;
    quint64 sequence_=0;
    void connectSocket();
    void receive(const QString &text);
    void lost();
};
class ObsConnections : public QObject {
    Q_OBJECT
public:
    explicit ObsConnections(Controller &controller,QObject *parent=nullptr);
    ~ObsConnections()override;
private:
    struct Endpoint;
    Controller &c_;
    QMap<QString,std::shared_ptr<Endpoint>> endpoints_;
    QTimer refresh_,poll_;
    QString activeProject_;
    std::function<void()> nativeReconnect_,nativePreflight_,nativeRefresh_;
    std::function<QJsonArray()> nativeScenes_;
    std::function<QSize()> nativeVideoSize_;
    std::function<void(const QString &,const QString &,bool,const QString &)> nativeAction_;
    static QString key(const QJsonObject &configuration);
    bool remote()const;
    std::shared_ptr<Endpoint> endpoint(bool create=false);
    void configure(const QJsonObject &configuration,const QString &password);
    void connectActive(const QString &password={});
    void projectChanged();
    void refreshScenes(const std::shared_ptr<Endpoint> &endpoint);
    void refreshOutputs();
    void poll(const std::shared_ptr<Endpoint> &endpoint);
    void associate(const std::shared_ptr<Endpoint> &endpoint,const QString &scene,const QString &category);
    void sceneAction(const QString &scene,const QString &action,bool enabled,const QString &category);
};
}
