#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QPointer>
class QScreen;
class QCloseEvent;
class QTimer;
namespace jp {
QJsonObject screenIdentity(QScreen *screen);
QRect visibleGeometry(QRect desired,const QList<QRect> &available);
class OperatorWindow : public QWidget {
    Q_OBJECT
public:
    OperatorWindow(QWidget *panel,QWidget *dockHost,const QString &root);
    ~OperatorWindow()override;
    bool autoOpen()const;
    void restoreStartup();
    void detach();
    void dock();
    void fullscreen();
    void restore();
    void toggleFullscreen();
    void settings(QWidget *parent);
    void persist();
    QString mode()const{return mode_;}
    QScreen *operationScreen()const;
signals:
    void modeChanged(const QString &mode);
    void warning(const QString &message);
protected:
    void closeEvent(QCloseEvent *)override;
    void moveEvent(QMoveEvent *)override;
    void resizeEvent(QResizeEvent *)override;
    void keyPressEvent(QKeyEvent *)override;
private:
    QWidget *panel_;
    QPointer<QWidget> dockHost_;
    QString path_,mode_="dock",beforeFullscreen_="window";
    QJsonObject preferences_;
    QRect normalGeometry_;
    QTimer *saveTimer_;
    bool shuttingDown_=false,changing_=false;
    void watchScreens();
    void recover();
    void scheduleSave();
};
}
