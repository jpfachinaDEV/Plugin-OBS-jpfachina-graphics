#include "controller.hpp"
#include "operator-panel.hpp"
#include "operator-window.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextStream>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QTabWidget>
#include <QKeyEvent>
#include <QThread>
#include <QFontDatabase>
#include <QMouseEvent>

static int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw jp::Error(QString("Line %1: %2").arg(__LINE__).arg(#x));}while(false)
#define THROWS(x) do{bool thrown=false;try{x;}catch(const jp::Error&){thrown=true;}CHECK(thrown);}while(false)
static void events(){QElapsedTimer t;t.start();while(t.elapsed()<80){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(2);}}
static QJsonObject media(const jp::Store &s,const QString &pid,const QString &id){for(const auto &v:s.project(pid)["media"].toArray())if(v.toObject()["id"].toString()==id)return v.toObject();throw jp::Error("Missing fixture");}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("JP operator validation");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI",10));
    try{
        QTemporaryDir temp;CHECK(temp.isValid());const auto root=temp.path();QImage image(400,160,QImage::Format_ARGB32);image.fill(QColor(70,210,170));CHECK(image.save(root+"/input.png"));
        jp::Store store(root+"/library");store.open();const auto pid=store.snapshot()["active"].toString();
        const auto a=store.importFile(pid,root+"/input.png","Logos"),b=store.duplicateMedia(pid,a),d=store.duplicateMedia(pid,a);
        CHECK(a!=b);CHECK(media(store,pid,b)["file"]==media(store,pid,a)["file"]);CHECK(media(store,pid,b)["live"].isNull());
        store.updateMedia(pid,a,{{"draft",QJsonObject{{"x",222},{"y",333},{"rotation",20},{"opacity",60},{"scale",1.25},{"anchor","center"}}}});store.setLive(pid,b,true);
        const auto before=media(store,pid,b);auto changes=store.synchronize(pid,a,{b,d},{"x","y","rotation","opacity"});
        CHECK(changes.size()==2);CHECK(media(store,pid,b)["draft"].toObject()["x"]==222);CHECK(media(store,pid,b)["live"]==before["live"]);CHECK(media(store,pid,b)["name"]==before["name"]);CHECK(media(store,pid,b)["file"]==before["file"]);CHECK(media(store,pid,b)["origin"]==before["origin"]);
        store.updateMedia(pid,b,{{"name","Renamed after sync"}});store.restoreLayouts(pid,changes,true);CHECK(media(store,pid,b)["draft"]==before["draft"]);CHECK(media(store,pid,b)["name"]=="Renamed after sync");store.restoreLayouts(pid,changes,false);CHECK(media(store,pid,b)["draft"].toObject()["x"]==222);
        store.updateMedia(pid,d,{{"draft",QJsonObject{{"x",999}}}});const auto snapshot=store.snapshot();THROWS(store.restoreLayouts(pid,changes,true));CHECK(store.snapshot()==snapshot);
        THROWS(store.synchronize(pid,a,{b,"missing"},{"x"}));CHECK(store.snapshot()==snapshot);THROWS(store.synchronize(pid,a,{b},{"text"}));THROWS(store.synchronize(pid,a,{a},{"x"}));CHECK(store.snapshot()==snapshot);
        THROWS(store.updateMedia(pid,a,{{"draft",QJsonObject{{"opacity",101}}}}));THROWS(store.updateMedia(pid,a,{{"draft",QJsonObject{{"width",30},{"cropLeft",40}}}}));CHECK(store.snapshot()==snapshot);
        const QJsonValue catId=store.project(pid)["categoryIds"].toObject()["Logos"];store.renameCategory(pid,"Logos","Marcas");CHECK(store.project(pid)["categoryIds"].toObject()["Marcas"]==catId);CHECK(media(store,pid,a)["category"]=="Marcas");store.addCategory(pid,"Patrocinadores");CHECK(store.project(pid)["categories"].toArray().contains("Patrocinadores"));
        const auto title=store.createTitle(pid,"Apresentador",{{"name","João Silva"},{"role","Repórter"},{"city","São Paulo"}});store.setLive(pid,title,true);const QJsonValue live=media(store,pid,title)["live"];store.updateMedia(pid,title,{{"text",QJsonObject{{"name","Maria"},{"role","Âncora"},{"city","Curitiba"}}}});CHECK(media(store,pid,title)["live"]==live);CHECK(media(store,pid,title)["draft"].toObject()["text"].toObject()["name"]=="Maria");
        store.saveTemplate(pid,"Position only",{{"x",17},{"y",42},{"opacity",50}});store.applyTemplate(pid,"Position only",{title});CHECK(media(store,pid,title)["draft"].toObject()["text"].toObject()["name"]=="Maria");CHECK(media(store,pid,title)["live"]==live);CHECK(media(store,pid,title)["draft"].toObject()["x"]==17);
        THROWS(store.resolution(pid,1280,720));store.clearLive(pid);store.resolution(pid,1280,720);CHECK(store.project(pid)["width"]==1280);
        const auto imported=store.importFile(pid,root+"/input.png","Banner");CHECK(media(store,pid,imported)["draft"].toObject()["y"].toDouble()>360);
        CHECK(QDir().mkpath(root+"/export"));store.exportProject(pid,root+"/export");const auto second=store.importProject(root+"/export");CHECK(second!=pid);CHECK(store.project(second)["media"].toArray().size()==5);for(const auto &cat:jp::panelCategories())CHECK(store.project(second)["categories"].toArray().contains(cat));
        for(const auto &v:store.project(second)["media"].toArray()){CHECK(v.toObject()["live"].isNull());CHECK(v.toObject()["id"].toString()!=a);}
        // Untrusted package paths must fail without committing a partial project.
        QFile manifest(root+"/export/project.jpgfx.json");CHECK(manifest.open(QIODevice::ReadOnly));auto doc=QJsonDocument::fromJson(manifest.readAll()).object();manifest.close();auto project=doc["project"].toObject();auto list=project["media"].toArray();auto entry=list[0].toObject();entry["file"]="media/../../input.png";list[0]=entry;project["media"]=list;doc["project"]=project;CHECK(manifest.open(QIODevice::WriteOnly|QIODevice::Truncate));manifest.write(QJsonDocument(doc).toJson());manifest.close();const auto safe=store.snapshot();THROWS(store.importProject(root+"/export"));CHECK(store.snapshot()==safe);
        store.selectProject(pid);jp::Downloader downloader(store);jp::Controller controller(store,downloader);controller.select(a);controller.edit({{"x",444}});events();CHECK(controller.dirty);CHECK(media(store,pid,a)["draft"].toObject()["x"]==222);THROWS(controller.take());controller.setObsState(true,false,false,"OBS test integrated");store.saveCategoryLayouts(pid,{{"Banner",jp::Store::validatedLayout(store.categoryLayout(pid,"Banner"),{{"x",444}})}},{1280,720});controller.take();events();CHECK(media(store,pid,a)["live"].toObject()["x"]==444);controller.edit({{"x",700}});CHECK(media(store,pid,a)["live"].toObject()["x"]==444);
        // Geometry recovery includes negative desktop coordinates and oversize windows.
        const QList<QRect> screens{QRect(-1920,0,1920,1080),QRect(0,0,1366,728)};CHECK(screens[0].contains(jp::visibleGeometry(QRect(-1800,10,1200,700),screens)));CHECK(screens[0].contains(jp::visibleGeometry(QRect(-9000,9000,8000,6000),screens))||screens[1].contains(jp::visibleGeometry(QRect(-9000,9000,8000,6000),screens)));
        QWidget dock;if(app.arguments().contains("--visual"))dock.setAttribute(Qt::WA_DontShowOnScreen);auto *layout=new QVBoxLayout(&dock);auto *panel=new jp::OperatorPanel(controller);layout->addWidget(panel);dock.resize(1366,768);dock.show();events();
        controller.save();panel->findChild<QPushButton*>("jpEditPosition")->click();events();CHECK(!controller.dirty);CHECK(media(store,pid,a)["draft"].toObject()["x"]==700);CHECK(media(store,pid,a)["live"].toObject()["x"]==444);
        {auto *canvas=panel->findChild<jp::PositionCanvas*>();CHECK(canvas);auto *position=panel->findChild<jp::PositionDialog*>();CHECK(position);position->session().edit({{"x",300},{"y",200},{"width",400},{"height",160},{"scale",1},{"rotation",0},{"anchor","center"}});events();
        const double canvasScale=qMin(canvas->width()/1280.0,canvas->height()/720.0);const QPointF canvasOrigin((canvas->width()-1280*canvasScale)/2,(canvas->height()-720*canvasScale)/2);const auto handle=canvasOrigin+QPointF(500,280)*canvasScale;const auto destination=handle+QPointF(100,40)*canvasScale;
        QMouseEvent press(QEvent::MouseButtonPress,handle,canvas->mapToGlobal(handle.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&press);
        QMouseEvent move(QEvent::MouseMove,destination,canvas->mapToGlobal(destination.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(canvas,&move);
        QMouseEvent release(QEvent::MouseButtonRelease,destination,canvas->mapToGlobal(destination.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(canvas,&release);
        CHECK(qAbs(position->session().layout()["width"].toDouble()-500)<.01);CHECK(qAbs(position->session().layout()["height"].toDouble()-200)<.01);CHECK(qAbs(position->session().layout()["x"].toDouble()-350)<.01);CHECK(qAbs(position->session().layout()["y"].toDouble()-220)<.01);CHECK(media(store,pid,a)["live"].toObject()["x"]==444);position->reject();}controller.edit({{"x",700}});
        {jp::OperatorWindow operation(panel,&dock,store.root());if(app.arguments().contains("--visual"))operation.setAttribute(Qt::WA_DontShowOnScreen);CHECK(operation.mode()=="dock");operation.detach();events();CHECK(operation.mode()=="window");CHECK(panel->parentWidget()==&operation);CHECK(controller.draft["x"]==700);CHECK(media(store,pid,a)["live"].toObject()["x"]==444);
            QKeyEvent f11(QEvent::KeyPress,Qt::Key_F11,Qt::NoModifier);QApplication::sendEvent(&operation,&f11);events();CHECK(operation.mode()=="fullscreen");QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&operation,&escape);events();CHECK(operation.mode()=="window");operation.dock();CHECK(panel->parentWidget()==&dock);CHECK(operation.mode()=="dock");operation.fullscreen();events();operation.restore();CHECK(operation.mode()=="dock");CHECK(panel->parentWidget()==&dock);
            operation.detach();events();operation.close();events();CHECK(operation.mode()=="dock");operation.persist();}
        CHECK(QFile::exists(store.root()+"/operator.json"));CHECK(controller.draft["x"]==700);CHECK(media(store,pid,a)["live"].toObject()["x"]==444);
        int completed=0,failed=0;QObject::connect(&store,&jp::Store::importFinished,&app,[&](const QString&,bool error){++completed;if(error)++failed;});
        for(int i=0;i<6;++i)store.importFileAsync(pid,root+"/input.png","Banner");QElapsedTimer importTimer;importTimer.start();while(completed<6&&importTimer.elapsed()<5000)events();CHECK(completed==6);CHECK(failed==0);const auto asynchronous=store.project(pid)["media"].toArray();CHECK(asynchronous.size()==11);CHECK(asynchronous.last().toObject()["file"]==asynchronous.first().toObject()["file"]);
        store.exportProjectAsync(pid,root+"/async-export");importTimer.restart();while(completed<7&&importTimer.elapsed()<5000)events();CHECK(completed==7);CHECK(failed==0);CHECK(QFile::exists(root+"/async-export/project.jpgfx.json"));const auto projectsBefore=store.snapshot()["projects"].toArray().size();store.importProjectAsync(root+"/async-export");importTimer.restart();while(completed<8&&importTimer.elapsed()<5000)events();CHECK(completed==8);CHECK(failed==0);CHECK(store.snapshot()["projects"].toArray().size()==projectsBefore+1);store.selectProject(pid);events();controller.select(a);controller.edit({{"x",700}});
        QDir().mkpath("artifacts");dock.resize(1366,768);events();CHECK(dock.grab().save("artifacts/qt-operator-1366.png"));dock.resize(500,920);events();CHECK(dock.grab().save("artifacts/qt-dock-500.png"));
        CHECK(dock.width()<=520);for(const auto &name:{"jpTake","jpOut","jpScenes","jpContent","jpLink","jpView"}){auto *button=panel->findChild<QPushButton*>(name);CHECK(button);CHECK(button->isVisible());CHECK(panel->rect().contains(button->mapTo(panel,QPoint(0,0))));}
        for(const auto &size:QList<QSize>{{1366,768},{1920,1080},{2560,1440},{3840,2160},{683,384},{500,450}}){dock.resize(size);events();CHECK(dock.size()==size);for(const auto &name:{"jpTake","jpOut","jpScenes","jpContent","jpLink","jpView"}){auto *control=panel->findChild<QPushButton*>(name);CHECK(control->isVisible());CHECK(panel->rect().contains(QRect(control->mapTo(panel,QPoint(0,0)),control->size())));}if(app.arguments().contains("--visual"))CHECK(dock.grab().save(QString("artifacts/qt-layout-%1x%2.png").arg(size.width()).arg(size.height())));}
        // Reopen schema 1 in a fresh profile; original bytes survive migration.
        const auto oldRoot=root+"/old";CHECK(QDir().mkpath(oldRoot));auto legacy=store.snapshot();legacy["schema"]=1;QFile old(oldRoot+"/library.json");CHECK(old.open(QIODevice::WriteOnly));const auto original=QJsonDocument(legacy).toJson();old.write(original);old.close();{jp::Store migrated(oldRoot);migrated.open();CHECK(migrated.snapshot()["schema"]==2);QFile backup(oldRoot+"/library.schema1.backup.json");CHECK(backup.open(QIODevice::ReadOnly));CHECK(backup.readAll()==original);}
        QTextStream(stdout)<<"PASS operator, synchronization, recovery, migration and project portability: "<<checks<<" checks\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}
}
