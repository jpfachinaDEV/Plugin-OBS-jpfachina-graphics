#pragma once
#include "controller.hpp"
#include <QDialog>
#include <QWidget>
#include <QImage>
#include <QMap>
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QTimer;

namespace jp {
class PositionSession : public QObject {
    Q_OBJECT
public:
    explicit PositionSession(Controller &controller,QObject *parent=nullptr);
    void open(const QString &category);
    void category(const QString &category);
    QString category()const{return category_;}
    QString projectId()const{return project_;}
    QSize frame()const{return frame_;}
    QJsonObject layout()const{return drafts_.value(category_);}
    QJsonObject media()const;
    QImage image()const{return image_;}
    bool dirty()const{return drafts_!=originals_;}
    void edit(const QJsonObject &patch);
    void move(double x,double y);
    void reset();
    void save(bool applyLive=false);
    void cancel();
signals:
    void changed();
private:
    Controller &c_;
    QString project_,category_,scoreboardId_;
    QSize frame_;
    QMap<QString,QJsonObject> originals_,drafts_;
    QImage image_;
    void loadImage();
};
class PositionCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PositionCanvas(PositionSession &session,QWidget *parent=nullptr);
    QRectF compositionRect()const;
    void setPreserveAspect(bool value){preserveAspect_=value;}
protected:
    void paintEvent(QPaintEvent *)override;
    void mousePressEvent(QMouseEvent *)override;
    void mouseMoveEvent(QMouseEvent *)override;
    void mouseReleaseEvent(QMouseEvent *)override;
    void hideEvent(QHideEvent *)override;
private:
    PositionSession &session_;
    QJsonObject start_;
    QPointF startPoint_;
    bool dragging_=false,resizing_=false,preserveAspect_=true;
    int handle_=-1;
};
class PositionDialog : public QDialog {
    Q_OBJECT
public:
    explicit PositionDialog(Controller &controller,QWidget *parent=nullptr);
    void openCategory(const QString &category);
    PositionSession &session(){return session_;}
    void reject()override;
private:
    Controller &c_;
    PositionSession session_;
    PositionCanvas *canvas_;
    QComboBox *category_,*step_,*anchor_,*alignment_;
    QCheckBox *aspect_;
    QMap<QString,QDoubleSpinBox*> fields_;
    QLabel *coordinates_,*status_;
    QTimer *refresh_;
    bool rendering_=false;
    void render();
    void run(const std::function<void()> &action);
    void nudge(int dx,int dy);
};
}
