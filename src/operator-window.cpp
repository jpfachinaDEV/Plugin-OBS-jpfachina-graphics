#include "operator-window.hpp"
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QShortcut>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <algorithm>

namespace jp {
QJsonObject screenIdentity(QScreen *s){if(!s)return {};return {{"name",s->name()},{"manufacturer",s->manufacturer()},{"model",s->model()},{"serial",s->serialNumber()}};}
QRect visibleGeometry(QRect desired,const QList<QRect> &available){
    if(available.isEmpty())return QRect(80,80,1100,700);
    QRect best=available.first();int area=-1;
    for(const auto &r:available){const auto i=r.intersected(desired);const int a=i.width()*i.height();if(a>area){area=a;best=r;}}
    desired.setSize(QSize(qBound(320,desired.width(),qMax(320,best.width())),qBound(240,desired.height(),qMax(240,best.height()))));
    desired.moveLeft(qBound(best.left(),desired.left(),best.right()-desired.width()+1));desired.moveTop(qBound(best.top(),desired.top(),best.bottom()-desired.height()+1));return desired;
}
OperatorWindow::OperatorWindow(QWidget *panel,QWidget *host,const QString &root):QWidget(nullptr,Qt::Window),panel_(panel),dockHost_(host),path_(root+"/operator.json"){
    setObjectName("jpOperatorWindow");setWindowTitle("JP Fachina Graphics · Central de operação");resize(1280,800);setMinimumSize(360,300);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    QFile f(path_);if(f.open(QIODevice::ReadOnly)&&f.size()<1024*1024){const auto doc=QJsonDocument::fromJson(f.readAll()).object();if(doc["version"]==1)preferences_=doc;}
    const auto g=preferences_["geometry"].toArray();if(g.size()==4)normalGeometry_=QRect(g[0].toInt(),g[1].toInt(),g[2].toInt(),g[3].toInt());
    saveTimer_=new QTimer(this);saveTimer_->setSingleShot(true);saveTimer_->setInterval(400);connect(saveTimer_,&QTimer::timeout,this,&OperatorWindow::persist);
    auto *f11=new QShortcut(QKeySequence(Qt::Key_F11),panel_);f11->setContext(Qt::WidgetWithChildrenShortcut);connect(f11,&QShortcut::activated,this,&OperatorWindow::toggleFullscreen);
    auto *escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);escape->setContext(Qt::WindowShortcut);connect(escape,&QShortcut::activated,this,[this]{if(mode_=="fullscreen"&&!QApplication::activeModalWidget())restore();});
    connect(qApp,&QGuiApplication::screenRemoved,this,[this](QScreen*){QTimer::singleShot(0,this,&OperatorWindow::recover);});
    connect(qApp,&QGuiApplication::screenAdded,this,[this](QScreen*){watchScreens();});watchScreens();
}
OperatorWindow::~OperatorWindow(){shuttingDown_=true;saveTimer_->stop();persist();if(panel_&&panel_->parentWidget()==this&&dockHost_){layout()->removeWidget(panel_);panel_->setParent(dockHost_);dockHost_->layout()->addWidget(panel_);}}
bool OperatorWindow::autoOpen()const{return preferences_["autoOpen"].toBool(false);}
QScreen *OperatorWindow::operationScreen()const{
    const auto wanted=preferences_["monitor"].toObject();QScreen *best=nullptr;int score=0;
    for(auto *s:QGuiApplication::screens()){const auto identity=screenIdentity(s);int n=0;
        for(const auto &k:{"serial","manufacturer","model","name"})if(!wanted[k].toString().isEmpty()&&wanted[k]==identity[k])n+=(QString(k)=="serial"?8:QString(k)=="name"?4:1);
        if(n>score){best=s;score=n;}}
    if(best)return best;const auto screens=QGuiApplication::screens();for(auto *s:screens)if(s!=QGuiApplication::primaryScreen())return s;return QGuiApplication::primaryScreen();
}
void OperatorWindow::watchScreens(){for(auto *s:QGuiApplication::screens()){connect(s,&QScreen::availableGeometryChanged,this,&OperatorWindow::recover,Qt::UniqueConnection);connect(s,&QScreen::geometryChanged,this,&OperatorWindow::recover,Qt::UniqueConnection);}}
void OperatorWindow::detach(){
    if(mode_=="fullscreen"){restore();return;}if(mode_=="window"){raise();activateWindow();return;}
    changing_=true;if(dockHost_)dockHost_->layout()->removeWidget(panel_);panel_->setParent(this);layout()->addWidget(panel_);
    mode_="window";setScreen(operationScreen());QList<QRect> areas;for(auto *s:QGuiApplication::screens())areas<<s->availableGeometry();
    const auto desired=normalGeometry_.isValid()?normalGeometry_:QRect(operationScreen()->availableGeometry().topLeft()+QPoint(30,30),QSize(1280,800));
    setGeometry(visibleGeometry(desired,areas));showNormal();panel_->show();raise();activateWindow();changing_=false;emit modeChanged(mode_);persist();
}
void OperatorWindow::dock(){
    if(!dockHost_||mode_=="dock")return;changing_=true;if(mode_=="window")normalGeometry_=geometry();
    layout()->removeWidget(panel_);panel_->setParent(dockHost_);dockHost_->layout()->addWidget(panel_);panel_->show();hide();mode_="dock";changing_=false;emit modeChanged(mode_);persist();
}
void OperatorWindow::fullscreen(){
    if(mode_=="fullscreen")return;const auto previous=mode_;if(mode_=="dock")detach();normalGeometry_=geometry();beforeFullscreen_=previous;
    changing_=true;setScreen(operationScreen());mode_="fullscreen";showFullScreen();changing_=false;emit modeChanged(mode_);persist();
}
void OperatorWindow::restore(){
    if(mode_!="fullscreen")return;changing_=true;showNormal();mode_="window";QList<QRect> areas;for(auto *s:QGuiApplication::screens())areas<<s->availableGeometry();setGeometry(visibleGeometry(normalGeometry_,areas));changing_=false;
    if(beforeFullscreen_=="dock")dock();else{emit modeChanged(mode_);persist();}
}
void OperatorWindow::toggleFullscreen(){if(mode_=="fullscreen")restore();else fullscreen();}
void OperatorWindow::restoreStartup(){if(!autoOpen())return;const auto requested=preferences_["mode"].toString();const auto savedMonitor=preferences_["monitor"].toObject();bool found=false;
    for(auto *s:QGuiApplication::screens())if(screenIdentity(s)==savedMonitor)found=true;
    if(requested=="fullscreen"&&found)fullscreen();else if(requested!="dock")detach();}
void OperatorWindow::recover(){
    if(shuttingDown_||changing_||mode_=="dock")return;
    bool selectedPresent=false;for(auto *s:QGuiApplication::screens())if(screenIdentity(s)==preferences_["monitor"].toObject())selectedPresent=true;
    if(mode_=="fullscreen"&&!selectedPresent){beforeFullscreen_="window";restore();emit warning("Monitor de operação desconectado. Janela restaurada na área disponível.");}
    if(mode_=="window"){QList<QRect> areas;for(auto *s:QGuiApplication::screens())areas<<s->availableGeometry();setGeometry(visibleGeometry(geometry(),areas));}
    scheduleSave();
}
void OperatorWindow::persist(){
    if(mode_=="window"&&!changing_)normalGeometry_=geometry();preferences_["version"]=1;preferences_["mode"]=mode_;
    if(preferences_["monitor"].toObject().isEmpty())preferences_["monitor"]=screenIdentity(operationScreen());
    preferences_["geometry"]=QJsonArray{normalGeometry_.x(),normalGeometry_.y(),normalGeometry_.width(),normalGeometry_.height()};
    QSaveFile f(path_);const auto b=QJsonDocument(preferences_).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())emit warning("Falha ao salvar preferências da janela.");
}
void OperatorWindow::scheduleSave(){if(!changing_&&!shuttingDown_&&mode_!="dock")saveTimer_->start();}
void OperatorWindow::moveEvent(QMoveEvent *e){QWidget::moveEvent(e);scheduleSave();}
void OperatorWindow::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);scheduleSave();}
void OperatorWindow::closeEvent(QCloseEvent *e){if(shuttingDown_){e->accept();return;}e->ignore();dock();}
void OperatorWindow::keyPressEvent(QKeyEvent *e){if(e->key()==Qt::Key_F11&&!QApplication::activeModalWidget()){toggleFullscreen();e->accept();return;}if(e->key()==Qt::Key_Escape&&mode_=="fullscreen"&&!QApplication::activeModalWidget()){restore();e->accept();return;}QWidget::keyPressEvent(e);}
void OperatorWindow::settings(QWidget *parent){
    QDialog dialog(parent);dialog.setWindowTitle("Monitor de Operação");auto *layout=new QVBoxLayout(&dialog);layout->addWidget(new QLabel("Selecione o monitor pela identificação do sistema:",&dialog));
    auto *screens=new QComboBox(&dialog);const auto list=QGuiApplication::screens();for(auto *s:list){screens->addItem(QString("%1 · %2 %3 · %4 × %5 · %6%7").arg(s->name(),s->manufacturer(),s->model()).arg(s->size().width()).arg(s->size().height()).arg(qRound(s->devicePixelRatio()*100)).arg(s==QGuiApplication::primaryScreen()?"% · Principal":"%"));if(s==operationScreen())screens->setCurrentIndex(screens->count()-1);}
    layout->addWidget(screens);auto *automatic=new QCheckBox("Restaurar o Modo Operador ao iniciar o OBS",&dialog);automatic->setChecked(autoOpen());layout->addWidget(automatic);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout->addWidget(buttons);
    if(dialog.exec()!=QDialog::Accepted)return;auto *selected=list.value(screens->currentIndex());if(selected&&!QGuiApplication::screens().contains(selected)){emit warning("Monitor desconectado durante a seleção.");return;}
    preferences_["monitor"]=screenIdentity(selected);preferences_["autoOpen"]=automatic->isChecked();if(mode_!="dock"){setScreen(selected);if(mode_=="fullscreen"){showNormal();showFullScreen();}else{normalGeometry_=QRect(selected->availableGeometry().topLeft()+QPoint(20,20),size());recover();}}persist();
}
}
