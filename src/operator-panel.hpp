#pragma once
#include "controller.hpp"
#include "position-editor.hpp"
#include <QWidget>
#include <QMap>
#include <functional>
#include <QImage>
class QComboBox;
class QLabel;
class QLineEdit;
class QTabWidget;
class QDoubleSpinBox;
class QCheckBox;
class QScrollArea;
class QTimer;
class QStackedWidget;
class QDialog;
class QPushButton;
class QGridLayout;
namespace jp {
class MediaCard;
class OperatorPanel : public QWidget {
    Q_OBJECT
public:
    OperatorPanel(Controller &controller,QWidget *parent=nullptr);
    ~OperatorPanel()override;
    QString category()const;
    void setDisplayMode(const QString &mode);
    QSize minimumSizeHint()const override{return QSize(340,260);}
signals:
    void windowRequested();
    void fullscreenRequested();
    void dockRequested();
    void monitorRequested();
protected:
    void resizeEvent(QResizeEvent *)override;
private:
    Controller &c_;
    QLabel *integration_,*status_,*logo_;
    QStackedWidget *pages_;
    PositionDialog *editorDialog_;
    QMap<QString,QList<MediaCard*>> cards_;
    QMap<QString,QGridLayout*> grids_;
    QMap<QString,QLabel*> headings_;
    QMap<QString,int> offsets_;
    QMap<QString,QWidget*> rows_;
    QMap<QString,QImage> thumbnailCache_;
    QPushButton *tab1_,*tab2_;
    QCheckBox *quickLoop_;
    QString activeCategory_="Logos";
    QString prefPath_;
    bool rendering_=false;
    QTimer *refreshTimer_;
    void render();
    void renderDraft();
    void renderCards();
    void run(const std::function<void()> &action);
    bool leaveDraft();
    void synchronize();
    void contentProperties();
    void importMedia();
    void download();

    void savePreferences();
    void editPosition();
    void contentManager();
    void scenesDialog();
    void linkDialog();
    void viewMenu();
    void arrangeCards();
    void showPage(int index);
    QImage thumbnail(const QJsonObject &media);
};
}
