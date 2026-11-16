#include "operator-panel.hpp"
#include "media-card.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QPushButton>
#include <QCheckBox>
#include <QDialog>
#include <QStackedWidget>
#include <QElapsedTimer>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QFontDatabase>

static int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw jp::Error(QString("Panel line %1: %2").arg(__LINE__).arg(#x));}while(false)
#define THROWS(x) do{bool thrown=false;try{x;}catch(const jp::Error&){thrown=true;}CHECK(thrown);}while(false)
static void events(){QElapsedTimer t;t.start();while(t.elapsed()<100){QCoreApplication::processEvents();QThread::msleep(2);}}
static QJsonObject media(jp::Store &s,const QString &pid,const QString &id){for(const auto &v:s.project(pid)["media"].toArray())if(v.toObject()["id"].toString()==id)return v.toObject();throw jp::Error("Fixture missing");}
int main(int argc,char **argv){QApplication app(argc,argv);QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");app.setFont(QFont("Segoe UI",10));try{
    QTemporaryDir temp;CHECK(temp.isValid());QImage image(400,160,QImage::Format_ARGB32);image.fill(QColor("#dec692"));CHECK(image.save(temp.path()+"/image.png"));QString pid,a,b,banner,qr,video;
    {
        jp::Store store(temp.path()+"/library");store.open();pid=store.snapshot()["active"].toString();CHECK(store.project(pid)["categories"].toArray().size()==6);
        a=store.importFile(pid,temp.path()+"/image.png","Logos");b=store.duplicateMedia(pid,a);banner=store.importFile(pid,temp.path()+"/image.png","Banner");qr=store.importFile(pid,temp.path()+"/image.png","QR-Codes");
        video=store.importFile(pid,QDir("tests/fixtures").absoluteFilePath("sample.mp4"),"Vídeos");
        jp::Downloader downloader(store);jp::Controller c(store,downloader);jp::OperatorPanel panel(c);if(app.arguments().contains("--visual"))panel.setAttribute(Qt::WA_DontShowOnScreen);panel.resize(1920,1080);panel.show();events();
        auto *pages=panel.findChild<QStackedWidget*>("jpPages");CHECK(pages&&pages->count()==2);CHECK(panel.findChildren<jp::MediaCard*>().size()==36);
        auto button=[&](const char *name){auto *b=panel.findChild<QPushButton*>(name);CHECK(b);return b;};
        auto card=[&](const QString &id){for(auto *item:panel.findChildren<jp::MediaCard*>())if(item->mediaId()==id)return item;throw jp::Error("Card missing");};
        for(auto *item:panel.findChildren<jp::MediaCard*>())if(item->isVisible())CHECK(qAbs(item->height()-item->width()*9./16)<=2);
        card(a)->click();events();CHECK(c.selectedId==a);CHECK(card(a)->property("state")=="selected");CHECK(media(store,pid,a)["live"].isNull());THROWS(c.take());
        c.setObsState(true,false,false,"Test adapter");c.take();events();CHECK(!c.onAir(a));CHECK(card(a)->property("state")=="selected");
        c.confirm({a});events();CHECK(card(a)->property("state")=="live");card(b)->click();events();CHECK(card(a)->property("state")=="live");CHECK(card(b)->property("state")=="selected");c.take();events();CHECK(media(store,pid,a)["live"].isNull());CHECK(card(a)->property("state")=="live");c.confirm({b});events();CHECK(card(a)->property("state")=="available");CHECK(card(b)->property("state")=="live");
        c.select(banner);c.take();c.select(qr);c.take();c.confirm({b,banner,qr});events();CHECK(card(b)->property("state")=="live");CHECK(card(banner)->property("state")=="live");CHECK(card(qr)->property("state")=="live");c.out();events();CHECK(card(qr)->property("state")=="live");CHECK(media(store,pid,b)["live"].isObject());CHECK(media(store,pid,banner)["live"].isObject());c.confirm({b,banner});events();CHECK(card(qr)->property("state")=="selected");
        QDir().mkpath("artifacts");CHECK(panel.grab().save("artifacts/panel-aba1-1920.png"));button("jpTab2")->click();events();CHECK(pages->currentIndex()==1);CHECK(c.onAir(b));c.select(video);events();auto *loop=panel.findChild<QCheckBox*>("jpQuickLoop");CHECK(loop&&loop->isVisible());loop->setChecked(false);events();CHECK(!media(store,pid,video)["draft"].toObject()["loop"].toBool());c.edit({{"endBehavior","out"}});c.save();c.select(b);c.select(video);CHECK(!c.draft["loop"].toBool());CHECK(c.draft["endBehavior"]=="out");CHECK(panel.grab().save("artifacts/panel-aba2-1920.png"));
        c.link(false);THROWS(c.take());THROWS(c.out());CHECK(!c.onAir(b));CHECK(media(store,pid,b)["live"].isObject());c.reconnect=[&]{c.setObsState(true,false,false,"Reconnected");c.confirm({b,banner});};c.link(true);CHECK(c.onAir(b));
        c.select(video);button("jpEditPosition")->click();events();auto *editor=panel.findChild<QDialog*>("jpPositionDialog");CHECK(editor&&editor->isVisible());CHECK(editor->findChild<jp::PositionCanvas*>()->internalWinId()==0);editor->reject();
        for(const auto &size:QList<QSize>{{1366,768},{1920,1080},{2560,1440},{683,384},{500,450}}){panel.resize(size);events();CHECK(panel.size()==size);for(const auto &name:{"jpTake","jpOut","jpContent","jpScenes","jpView"}){auto *b=button(name);CHECK(b->isVisible());CHECK(panel.rect().contains(QRect(b->mapTo(&panel,QPoint(0,0)),b->size())));}if(size.width()>=900)for(auto *item:panel.findChildren<jp::MediaCard*>())if(item->isVisible())CHECK(qAbs(item->height()-item->width()*9./16)<=2);}
        panel.resize(1366,768);events();CHECK(panel.grab().save("artifacts/panel-aba2-1366.png"));
        const auto before=media(store,pid,b);THROWS(store.replaceMedia(pid,b,temp.path()+"/image.png"));CHECK(media(store,pid,b)==before);c.select(qr);store.replaceMedia(pid,qr,temp.path()+"/image.png");CHECK(media(store,pid,qr)["id"]==qr);CHECK(QFile::exists(temp.path()+"/image.png"));
        // A queued replacement must recheck live state when the copied file is committed.
        int completed=0,failed=0;QObject::connect(&store,&jp::Store::importFinished,&panel,[&](const QString &,bool error){++completed;if(error)++failed;});const auto originalFile=media(store,pid,qr)["file"];
        store.replaceMediaAsync(pid,qr,temp.path()+"/image.png");store.take(pid,qr);QElapsedTimer timer;timer.start();while(completed<1&&timer.elapsed()<5000)events();CHECK(completed==1);CHECK(failed==1);CHECK(media(store,pid,qr)["file"]==originalFile);
        store.setLive(pid,qr,false);store.replaceMediaAsync(pid,qr,temp.path()+"/image.png");timer.restart();while(completed<2&&timer.elapsed()<5000)events();CHECK(completed==2);CHECK(failed==1);CHECK(QFile::exists(store.mediaPath(media(store,pid,qr))));CHECK(QFile::exists(temp.path()+"/image.png"));
    }
    {jp::Store store(temp.path()+"/library");store.open();CHECK(!media(store,pid,video)["draft"].toObject()["loop"].toBool());CHECK(media(store,pid,video)["draft"].toObject()["endBehavior"]=="out");CHECK(QFile::exists(store.root()+"/panel.json"));}
    QTextStream(stdout)<<"PASS two-tab panel, confirmed tally, category replacement, transport, loop and persistence: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){QTextStream(stderr)<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";return 1;}}
