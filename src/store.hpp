#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>
#include <memory>
#include <stdexcept>
#include <QSize>

namespace jp {
class Error : public std::runtime_error {
public:
    explicit Error(const QString &s) : std::runtime_error(s.toUtf8().constData()) {}
};
QString randomKey();
QString mediaType(const QByteArray &head);
bool publicAddress(const QString &address);
QString panelCategory(const QString &category,const QString &mime={});
QStringList panelCategories();
QStringList positionFields();
QJsonObject defaultPosition(const QString &category,QSize frame);
QJsonObject positionForFrame(QJsonObject layout,QSize from,QSize to);
QJsonObject effectiveLayout(const QJsonObject &project,const QJsonObject &media);
class Store;
QJsonObject compositionState(Store &store,const QJsonObject &project,const QString &selected={},const QJsonObject &draft={});
class Store : public QObject {
    Q_OBJECT
public:
    explicit Store(QString root, QObject *parent = nullptr);
    ~Store()override;
    void open();
    QJsonObject snapshot() const { return data_; }
    QJsonObject project(const QString &id) const;
    QJsonObject projectByKey(const QString &key) const;
    QJsonObject liveState(const QString &key) const;
    QString createProject(const QString &name);
    void renameProject(const QString &id, const QString &name);
    void removeProject(const QString &id);
    void selectProject(const QString &id);
    QString duplicateProject(const QString &id,const QString &name);
    QJsonObject categoryLayout(const QString &id,const QString &category,QSize frame={})const;
    QJsonObject defaultCategoryLayout(const QString &id,const QString &category,QSize frame={})const;
    void saveCategoryLayouts(const QString &id,const QJsonObject &layouts,QSize frame,bool applyLive=false);
    QJsonArray synchronizeCategories(const QString &id,const QString &source,const QStringList &targets,const QStringList &fields);
    void restoreCategoryLayouts(const QString &id,const QJsonArray &changes,bool undo);
    void obsLink(const QString &id,const QJsonObject &settings);
    void obsBindings(const QString &id,const QJsonArray &bindings);
    void categories(const QString &id, const QJsonArray &values);
    void renameCategory(const QString &id, const QString &name, const QString &replacement);
    void addCategory(const QString &id, const QString &name);
    void resolution(const QString &id, int width, int height);
    QString importFile(const QString &id, const QString &path, const QString &category, const QString &origin = {});
    void importFileAsync(const QString &id,const QString &path,const QString &category,const QString &origin={},bool removeSource=false);
    void updateMedia(const QString &projectId, const QString &id, const QJsonObject &patch);
    void setLive(const QString &projectId, const QString &id, bool on);
    void take(const QString &projectId,const QString &id);
    void replaceMedia(const QString &projectId,const QString &id,const QString &path);
    void replaceMediaAsync(const QString &projectId,const QString &id,const QString &path);
    void clearLive(const QString &id);
    void removeMedia(const QString &projectId, const QString &id);
    QString duplicateMedia(const QString &projectId, const QString &id);
    QString createTitle(const QString &projectId, const QString &name, const QJsonObject &text);
    QJsonArray synchronize(const QString &projectId, const QString &source, const QStringList &targets, const QStringList &properties);
    void restoreLayouts(const QString &projectId, const QJsonArray &changes, bool undo);
    void saveTemplate(const QString &projectId, const QString &name, const QJsonObject &layout);
    void applyTemplate(const QString &projectId, const QString &name, const QStringList &targets);
    void exportProject(const QString &id, const QString &directory) const;
    QString importProject(const QString &directory);
    void exportProjectAsync(const QString &id,const QString &directory);
    void importProjectAsync(const QString &directory);
    static QJsonObject validatedLayout(const QJsonObject &base, const QJsonObject &patch);
    void saveThumbnail(const QString &projectId, const QString &id, const QByteArray &png);
    QString mediaPath(const QJsonObject &media, bool thumbnail = false) const;
    QString root() const { return root_; }
    quint16 preferredPort() const;
    void port(quint16 p);
signals:
    void changed();
    void importFinished(const QString &message,bool error);
private:
    QString root_;
    QJsonObject data_;
    std::unique_ptr<QLockFile> lock_;
    void commit(const QJsonObject &next);
    void replaceProject(const QJsonObject &p);
    QString adoptFile(const QString &project,const QJsonObject &prepared,const QString &category,const QString &origin);
    void adoptReplacement(const QString &project,const QString &id,const QJsonObject &prepared,const QString &origin);
    QList<QStringList> importQueue_;
    int importing_=0;
    bool closing_=false;
    bool projectJob_=false;
    void startNextImport();
};
}
