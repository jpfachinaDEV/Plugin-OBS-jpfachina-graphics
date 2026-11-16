#pragma once
#include "store.hpp"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QPointer>
#include <QSet>
#include <functional>

namespace jp {
class Downloader : public QObject {
    Q_OBJECT
public:
    explicit Downloader(Store &store,QObject *parent=nullptr);
    ~Downloader() override;
    void fetch(const QString &pid,const QString &url,const QString &category);
signals:
    void completed(const QString &message,bool error);
private:
    Store &store_;
    QNetworkAccessManager manager_;
    QSet<QNetworkReply*> replies_;
    QSet<int> lookups_;
    int active_=0;
    bool stopping_=false;
    void resolve(const QUrl &url,std::function<void(QString)> callback);
};
}
