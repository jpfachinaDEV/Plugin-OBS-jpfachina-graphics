#pragma once
#include "store.hpp"
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QUrl>

namespace jp {
class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(Store &store, QObject *parent=nullptr);
    ~Server() override { stop(); }
    void start();
    void stop();
    QString baseUrl() const;
    QString panelUrl() const { return baseUrl()+"/panel#"+token_; }
    QString overlayUrl(const QString &id) const;
    QString token() const { return token_; }
signals:
    void nativeAction(const QString &action, const QString &projectId, const QString &category);
    void downloadRequested(const QString &projectId, const QString &url, const QString &category);
public slots:
    void notice(const QString &message, bool error=false);
private:
    struct Client { QByteArray input; QString stream; bool panel=false, busy=false; };
    Store &store_;
    QTcpServer tcp_;
    QHash<QTcpSocket*,Client> clients_;
    QString token_;
    QTimer heartbeat_;
    void read(QTcpSocket *s);
    void route(QTcpSocket *s, const QByteArray &method, const QUrl &url, const QHash<QByteArray,QByteArray> &headers, const QByteArray &body);
    void reply(QTcpSocket *s,int status,const QByteArray &body,const QByteArray &type="application/json",const QByteArray &extra={});
    void file(QTcpSocket *s,const QString &path,const QByteArray &mime,const QByteArray &range);
    QJsonObject command(const QJsonObject &in);
    void broadcast();
};
}
