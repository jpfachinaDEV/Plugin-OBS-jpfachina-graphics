#include "store.hpp"
#include "server.hpp"
#include "downloader.hpp"
#include <QApplication>
#include <QTextStream>
#include <QElapsedTimer>
#include <QThread>
#include <QTemporaryDir>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QBuffer>
#include <QDateTime>
#include <QTcpServer>

static int checks=0;
#define CHECK(expression) do{++checks;if(!(expression))throw jp::Error(QString("CHECK failed at line %1: %2").arg(__LINE__).arg(#expression));}while(false)
#define CHECK_EQUAL(a,b) CHECK((a)==(b))
#define CHECK_THROWS(expression,type) do{bool caught=false;try{expression;}catch(const type&){caught=true;}CHECK(caught);}while(false)
static bool waitFor(const std::function<bool()> &predicate,int ms){QElapsedTimer t;t.start();while(!predicate()&&t.elapsed()<ms){QCoreApplication::processEvents(QEventLoop::AllEvents,10);QThread::msleep(5);}return predicate();}
class CoreTest {
    static QString image(const QString &root){const auto p=root+"/input.png";QImage i(400,160,QImage::Format_ARGB32);i.fill(Qt::white);if(!i.save(p))throw jp::Error("Fixture PNG failed");return p;}
    static QString active(const jp::Store &s){return s.snapshot()["active"].toString();}
    static QJsonObject item(const jp::Store &s,const QString &p){return s.project(p)["media"].toArray().first().toObject();}
    struct Response {int status;QByteArray bytes;QByteArray range;};
    static Response get(QNetworkAccessManager &manager,const QString &url,const QByteArray &auth={},const QByteArray &origin={},const QByteArray &range={}){
        QNetworkRequest req{QUrl(url)};if(!auth.isEmpty())req.setRawHeader("Authorization",auth);if(!origin.isEmpty())req.setRawHeader("Origin",origin);if(!range.isEmpty())req.setRawHeader("Range",range);
        auto *reply=manager.get(req);QEventLoop loop;QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QTimer::singleShot(3000,&loop,&QEventLoop::quit);loop.exec();
        Response r{reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),reply->readAll(),reply->rawHeader("Content-Range")};reply->abort();reply->deleteLater();return r;
    }
public:
    void persistenceIsolationAndDraft(){
        QTemporaryDir tmp;QString first,second,key,id;
        {
            jp::Store s(tmp.path());s.open();first=active(s);key=s.project(first)["key"].toString();id=s.importFile(first,image(tmp.path()),"Logos");
            s.setLive(first,id,true);const auto initial=item(s,first)["live"].toObject();
            s.updateMedia(first,id,{{"draft",QJsonObject{{"x",345},{"width",700}}}});
            CHECK_EQUAL(item(s,first)["live"].toObject(),initial);CHECK_EQUAL(item(s,first)["draft"].toObject()["x"].toInt(),345);
            const auto generation=item(s,first)["generation"].toInt();s.setLive(first,id,true);
            CHECK_EQUAL(item(s,first)["live"].toObject()["x"].toInt(),345);CHECK_EQUAL(item(s,first)["generation"].toInt(),generation);
            second=s.createProject("Evento B");CHECK_EQUAL(s.project(second)["media"].toArray().size(),0);
            s.clearLive(second);CHECK(item(s,first)["live"].isObject());
        }
        {
            jp::Store s(tmp.path());s.open();CHECK_EQUAL(s.project(first)["key"].toString(),key);CHECK_EQUAL(active(s),second);
            CHECK(item(s,first)["live"].isObject());CHECK(QFile::exists(s.mediaPath(item(s,first))));
            s.removeProject(second);CHECK_EQUAL(s.project(first)["media"].toArray().size(),1);
            s.setLive(first,id,false);CHECK(item(s,first)["live"].isNull());s.setLive(first,id,true);
            CHECK_EQUAL(item(s,first)["generation"].toInt(),2);
        }
    }
    void atomicErrorsAndLocking(){
        QTemporaryDir tmp;jp::Store s(tmp.path());s.open();const auto p=active(s),id=s.importFile(p,image(tmp.path()),"Logos");const auto before=s.snapshot();
        CHECK_THROWS(s.updateMedia(p,id,{{"draft",QJsonObject{{"width",-1}}}}),jp::Error);
        CHECK_EQUAL(s.snapshot(),before);CHECK_THROWS(s.renameProject(p,""),jp::Error);
        jp::Store another(tmp.path());CHECK_THROWS(another.open(),jp::Error);
        CHECK_THROWS(s.updateMedia(p,id,{{"category","missing"}}),jp::Error);
        CHECK_THROWS(s.projectByKey(QString(64,'z')),jp::Error);
    }
    void corruptionPreserved(){
        QTemporaryDir tmp;QFile f(tmp.path()+"/library.json");CHECK(f.open(QIODevice::WriteOnly));f.write("not json");f.close();
        jp::Store s(tmp.path());CHECK_THROWS(s.open(),jp::Error);CHECK(f.open(QIODevice::ReadOnly));CHECK_EQUAL(f.readAll(),QByteArray("not json"));
    }
    void categoryRenameAndThumbnail(){
        QTemporaryDir tmp;jp::Store s(tmp.path());s.open();const auto p=active(s),id=s.importFile(p,image(tmp.path()),"Logos");
        auto cats=s.project(p)["categories"].toArray();cats[0]="Patrocinadores";s.categories(p,cats);CHECK_EQUAL(item(s,p)["category"].toString(),QString("Patrocinadores"));
        QImage thumb(320,128,QImage::Format_ARGB32);thumb.fill(Qt::white);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);CHECK(thumb.save(&buffer,"PNG"));
        s.saveThumbnail(p,id,bytes);CHECK(QFile::exists(s.mediaPath(item(s,p),true)));
        CHECK_THROWS(s.saveThumbnail(p,id,"not a png"),jp::Error);
        CHECK_THROWS(s.mediaPath(QJsonObject{{"file","media/../../secret"}}),jp::Error);
    }
    void signatureValidation(){
        CHECK_EQUAL(jp::mediaType(QByteArray::fromHex("89504e470d0a1a0a")),QString("image/png"));
        CHECK_EQUAL(jp::mediaType("GIF89a........"),QString("image/gif"));
        CHECK_EQUAL(jp::mediaType("RIFF....WEBP...."),QString("image/webp"));
        CHECK(jp::mediaType("<html>not media</html>").isEmpty());
        CHECK(jp::mediaType(QByteArray::fromHex("0000001866747970")+"avif0000avif").isEmpty());
    }
    void downloadAddressPolicy(){
        for(const auto &s:QStringList{"127.0.0.1","10.1.2.3","192.168.0.1","172.16.4.1","169.254.169.254","100.64.0.1","::1","fc00::1","fe80::1","::ffff:127.0.0.1","2001:db8::1","2002:7f00:1::"})CHECK(!jp::publicAddress(s));
        CHECK(jp::publicAddress("8.8.8.8"));CHECK(jp::publicAddress("2606:4700:4700::1111"));
    }
    void serverAuthRangesAndIsolation(){
        QTemporaryDir tmp;jp::Store s(tmp.path());s.open();s.port(28748);const auto p=active(s),id=s.importFile(p,image(tmp.path()),"Logos");s.setLive(p,id,true);
        jp::Server server(s);server.start();QNetworkAccessManager manager;manager.setProxy(QNetworkProxy::NoProxy);
        const auto base=server.baseUrl();CHECK_EQUAL(get(manager,base+"/api/state").status,401);
        const auto auth=("Bearer "+server.token()).toLatin1();CHECK_EQUAL(get(manager,base+"/api/state",auth).status,200);
        CHECK_EQUAL(get(manager,base+"/api/state",auth,"https://attacker.invalid").status,403);
        const auto key=s.project(p)["key"].toString();const auto live=get(manager,base+"/state/"+key);CHECK_EQUAL(live.status,200);
        const auto liveJson=QJsonDocument::fromJson(live.bytes).object();CHECK_EQUAL(liveJson["items"].toArray().size(),1);CHECK(!live.bytes.contains("origin"));CHECK(!live.bytes.contains("draft"));
        const auto range=get(manager,base+"/media/"+key+"/"+id,{}, {},"bytes=0-7");CHECK_EQUAL(range.status,206);CHECK_EQUAL(range.bytes,QByteArray::fromHex("89504e470d0a1a0a"));
        CHECK_EQUAL(get(manager,base+"/media/"+key+"/"+id,{}, {},"bytes=9999999999-").status,416);
        CHECK_EQUAL(get(manager,base+"/media/"+key+"/missing").status,400);
        const auto other=s.createProject("Other");const auto otherKey=s.project(other)["key"].toString();CHECK_EQUAL(get(manager,base+"/media/"+otherKey+"/"+id).status,400);
        const auto branding=get(manager,base+"/branding.png");CHECK_EQUAL(branding.status,200);CHECK_EQUAL(QCryptographicHash::hash(branding.bytes,QCryptographicHash::Sha256).toHex(),QByteArray("49e0bd162507b25133d594a42729b2d711fcbc8fe34d475d163f17e6b8c309ff"));
        server.stop();QTcpServer probe;CHECK(probe.listen(QHostAddress::LocalHost,28748));
    }
    void downloaderBlocksLocalAndSchemes(){
        QTemporaryDir tmp;jp::Store s(tmp.path());s.open();jp::Downloader downloader(s);QList<QVariantList> spy;
        QObject::connect(&downloader,&jp::Downloader::completed,&downloader,[&](const QString &m,bool e){spy.append(QVariantList{m,e});});
        downloader.fetch(active(s),"file:///C:/Windows/win.ini","Outros");CHECK_EQUAL(spy.size(),1);CHECK(spy.last()[1].toBool());
        downloader.fetch(active(s),"http://127.0.0.1/private","Outros");CHECK(waitFor([&]{return spy.size()==2;},5000));CHECK(spy.last()[1].toBool());
        CHECK_EQUAL(s.project(active(s))["media"].toArray().size(),0);
    }
    void scoreboardWorkflow(){
        QTemporaryDir home,other;CHECK(home.isValid()&&other.isValid());const auto transparent=home.path()+"/transparent.png",opaque=home.path()+"/opaque.png";
        QImage a(640,180,QImage::Format_ARGB32);a.fill(Qt::transparent);CHECK(a.save(transparent,"PNG"));QImage b(640,180,QImage::Format_RGB32);b.fill(Qt::white);CHECK(b.save(opaque,"PNG"));
        QString custom,projectId;{
            jp::Store store(home.path());store.open();projectId=active(store);CHECK_EQUAL(store.scoreboards().size(),5);
            const QJsonObject name{{"id","team1.name"},{"x",.05},{"y",.2},{"w",.3},{"h",.5},{"size",52},{"font","Segoe UI"},{"color","#ffffff"},{"align","left"}};
            custom=store.saveScoreboard({{"name","Campeonato Colina"},{"width",640},{"height",180},{"fields",QJsonArray{name}}},transparent);CHECK_EQUAL(store.scoreboards().size(),6);CHECK(QFile::exists(home.path()+"/scoreboard-thumbs/"+custom+".png"));
            auto edited=store.scoreboards().last().toObject();auto fields=edited["fields"].toArray();auto moved=fields.first().toObject();moved["x"]=.15;fields[0]=moved;edited["fields"]=fields;edited["name"]="Campeonato Colina 2026";store.saveScoreboard(edited);CHECK(store.scoreboards().last().toObject()["name"]=="Campeonato Colina 2026");CHECK(store.scoreboards().last().toObject()["fields"].toArray().first().toObject()["x"].toDouble()==.15);
            const auto second=store.saveScoreboard({{"name","PNG opaco"},{"width",640},{"height",180},{"fields",QJsonArray{}}},opaque);CHECK(!second.isEmpty());
            CHECK_THROWS(store.saveScoreboard({{"name","Inválido"},{"width",640},{"height",180},{"fields",QJsonArray{QJsonObject{{"id","script.run"}}}}},transparent),jp::Error);
            const auto mediaId=store.selectScoreboard(projectId,custom);store.scoreboardData(projectId,{{"team1.name","COLINA"},{"team2.name","BARRETOS"},{"team1.score","2"},{"team2.score","0"},{"clockSeconds",2700}});
            store.take(projectId,mediaId);const auto media=store.project(projectId)["media"].toArray().first().toObject();CHECK(media["live"].isObject());const auto first=store.scoreboardImagePath(store.project(projectId),media);CHECK(QFile::exists(first));
            const auto banner=store.importFile(projectId,opaque,"Banner");store.take(projectId,banner);CHECK(store.project(projectId)["media"].toArray().first().toObject()["live"].isObject());store.take(projectId,mediaId);CHECK(store.project(projectId)["media"].toArray().last().toObject()["live"].isObject());
            const auto templateBefore=store.scoreboards().at(5).toObject();store.scoreboardData(projectId,{{"team1.score","3"}});CHECK(first!=store.scoreboardImagePath(store.project(projectId),media));CHECK(store.project(projectId)["media"].toArray().first().toObject()["live"].isObject());CHECK(store.scoreboards().at(5).toObject()==templateBefore);const auto scorePath=store.scoreboardImagePath(store.project(projectId),media);store.scoreboardData(projectId,{{"clockStartedAt",double(QDateTime::currentMSecsSinceEpoch()-2000)},{"clockRunning",true}});CHECK(scorePath!=store.scoreboardImagePath(store.project(projectId),media));store.scoreboardData(projectId,{{"clockRunning",false}});
            CHECK_THROWS(store.removeScoreboard(custom),jp::Error);store.exportScoreboard(custom,home.path()+"/board.zip");CHECK(QFile::exists(home.path()+"/board.zip"));
            store.exportProject(projectId,home.path()+"/project-export");CHECK(QFile::exists(home.path()+"/project-export/project.jpgfx.json"));
            store.setLive(projectId,mediaId,false);store.removeMedia(projectId,mediaId);store.removeScoreboard(custom);CHECK_EQUAL(store.scoreboards().size(),6);const auto clone=store.duplicateScoreboard("builtin-2");CHECK(!clone.startsWith("builtin-"));CHECK(QFile::exists(home.path()+"/scoreboards/"+clone+".png"));store.removeScoreboard(clone);
        }
        {
            jp::Store store(home.path());store.open();CHECK_EQUAL(store.scoreboards().size(),6);CHECK_THROWS(store.importProject(home.path()+"/project-export"),jp::Error);
        }
        QString otherPid,transportedPid;{
            jp::Store store(other.path());store.open();const auto imported=store.importScoreboard(home.path()+"/board.zip");CHECK(!imported.isEmpty());CHECK(imported==custom);CHECK_EQUAL(store.scoreboards().size(),6);CHECK(QFile::exists(other.path()+"/scoreboards/"+imported+".png"));
            const auto duplicate=store.importScoreboard(home.path()+"/board.zip");CHECK(duplicate!=imported);store.removeScoreboard(duplicate);
            otherPid=active(store);const auto mediaId=store.selectScoreboard(otherPid,imported);store.scoreboardData(otherPid,{{"team1.name","OUTRO TIME"},{"team1.score","1"}});store.setLive(otherPid,mediaId,true);
            CHECK_THROWS(store.importScoreboard(opaque),jp::Error);QFile malicious(other.path()+"/bad.zip");CHECK(malicious.open(QIODevice::WriteOnly));malicious.write("PK\003\004garbage");malicious.close();CHECK_THROWS(store.importScoreboard(malicious.fileName()),jp::Error);
            QFile package(home.path()+"/board.zip");CHECK(package.open(QIODevice::ReadOnly));auto tampered=package.readAll();package.close();tampered.replace("base.png","evil.exe");QFile attack(other.path()+"/attack.zip");CHECK(attack.open(QIODevice::WriteOnly));attack.write(tampered);attack.close();CHECK_THROWS(store.importScoreboard(attack.fileName()),jp::Error);
            transportedPid=store.importProject(home.path()+"/project-export");CHECK(store.project(transportedPid)["scoreboardData"].toObject()["team1.score"]=="3");CHECK(store.project(transportedPid)["media"].toArray().first().toObject()["templateId"]==custom);
        }
        {jp::Store store(other.path());store.open();CHECK_EQUAL(store.scoreboards().size(),6);const auto p=store.project(otherPid);CHECK(p["scoreboardData"].toObject()["team1.name"]=="OUTRO TIME");const auto m=p["media"].toArray().first().toObject();CHECK(m["live"].isObject());CHECK(QFile::exists(store.scoreboardImagePath(p,m)));CHECK(store.project(transportedPid)["media"].toArray().first().toObject()["live"].isNull());}
    }
};
int main(int argc,char **argv){
    QApplication app(argc,argv);CoreTest test;int passed=0;
    const std::pair<const char*,void(CoreTest::*)()> tests[]={
        {"persistence and isolation",&CoreTest::persistenceIsolationAndDraft},{"atomic errors and lock",&CoreTest::atomicErrorsAndLocking},
        {"corrupt library preserved",&CoreTest::corruptionPreserved},{"categories and thumbnails",&CoreTest::categoryRenameAndThumbnail},
        {"media signatures",&CoreTest::signatureValidation},{"download address policy",&CoreTest::downloadAddressPolicy},
        {"HTTP authentication, isolation, ranges, branding and shutdown",&CoreTest::serverAuthRangesAndIsolation},
        {"downloader blocks local URLs and schemes",&CoreTest::downloaderBlocksLocalAndSchemes},
        {"scoreboard import, persistence, take, updates and package",&CoreTest::scoreboardWorkflow}};
    for(const auto &t:tests){try{(test.*t.second)();++passed;QTextStream(stdout)<<"PASS "<<t.first<<"\n";}catch(const std::exception &e){QTextStream(stderr)<<"FAIL "<<t.first<<": "<<e.what()<<"\n";return 1;}}
    QTextStream(stdout)<<passed<<" tests, "<<checks<<" checks passed\n";return 0;
}
