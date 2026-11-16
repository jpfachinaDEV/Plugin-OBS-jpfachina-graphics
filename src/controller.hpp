#pragma once
#include "store.hpp"
#include "downloader.hpp"
#include <QStringList>
#include <functional>
#include <QSet>
#include <QMap>

namespace jp {
class Controller : public QObject {
    Q_OBJECT
public:
    Controller(Store &store,Downloader &downloader,QObject *parent=nullptr);
    Store &store;
    Downloader &downloader;
    QString selectedId;
    QJsonObject draft;
    bool dirty=false,online=false,streaming=false,recording=false;
    QString obsStatus="OBS indisponível";
    std::function<void()> preflightTake;
    std::function<QJsonArray()> scenes;
    std::function<void(const QString &,const QString &,bool,const QString &)> sceneAction;
    std::function<void()> reconnect;
    std::function<QSize()> videoSize;
    std::function<void(const QJsonObject &,const QString &)> connectionRequested;
    std::function<void()> scenesRefresh;
    QJsonArray obsScenes;
    QMap<QString,QSet<QString>> confirmedProjects;
    void confirmProjects(const QMap<QString,QSet<QString>> &ids,const QMap<QString,QMap<QString,int>> &generations={});
    void confirmProvider(const QString &provider,const QMap<QString,QSet<QString>> &ids,const QMap<QString,QMap<QString,int>> &generations={});
    void switchProject(const QString &id);
    QStringList otherProjectsOnAir()const;
    bool linked=true;
    QSet<QString> confirmed;
    bool onAir(const QString &id)const{return online&&confirmed.contains(id);}
    void confirm(const QSet<QString> &ids,const QMap<QString,int> &generations={});
    void link(bool enabled);
    QJsonObject project() const;
    QJsonObject selected() const;
    QString projectId() const;
    void select(const QString &id);
    void edit(const QJsonObject &patch);
    void save();
    void take();
    void out();
    void sync(const QString &source,const QStringList &targets,const QStringList &fields);
    void syncCategories(const QString &source,const QStringList &targets,const QStringList &fields);
    void undo();
    void redo();
    void setObsState(bool integrated,bool stream,bool record,const QString &status);
    void notify(const QString &message,bool error=false);
    QStringList history;
signals:
    void changed();
    void draftChanged();
    void notice(const QString &message,bool error);
    void selectionChanged();
    void obsScenesChanged();
private:
    struct Operation {QString project;QJsonArray changes;bool categories=false;};
    QList<Operation> undo_,redo_;
    QString lastProject_;
    bool refreshPending_=false;
    QString pendingId_,pendingProject_,pendingName_;
    int pendingGeneration_=0;
    bool pendingOn_=false;
    QMap<QString,QMap<QString,QSet<QString>>> providers_;
    QMap<QString,QMap<QString,QMap<QString,int>>> providerGenerations_;
    void refresh();
};
}
