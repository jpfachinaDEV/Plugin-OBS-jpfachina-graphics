#include "operator-panel.hpp"
#include "position-editor.hpp"
#include "obs-websocket.hpp"
#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>

// Render the actual production widgets against an authenticated, isolated OBS.
// No mock tally and no connection to a production profile are used here.
static bool waitFor(const std::function<bool()> &condition,int timeout=30000){
    QElapsedTimer time;time.start();while(!condition()&&time.elapsed()<timeout){QCoreApplication::processEvents();QThread::msleep(5);}return condition();
}
static void settle(int ms=250){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(5);}}
static void require(bool ok,const QString &message){if(!ok)throw jp::Error(message);}
static QJsonObject rpc(jp::ObsWebSocket &client,const QString &type,const QJsonObject &arguments={}){
    bool done=false;QString error;QJsonObject value;client.request(type,arguments,[&](QJsonObject result,QString failure){value=result;error=failure;done=true;});
    require(waitFor([&]{return done;}),"OBS request timed out");require(error.isEmpty(),error);return value;
}
static void label(QPainter &p,const QRect &area,const QString &text,int size,const QColor &color=Qt::white,int weight=QFont::Bold){
    p.setPen(color);p.setFont(QFont("Segoe UI",size,weight));p.drawText(area,Qt::AlignLeft|Qt::AlignVCenter,text);
}
static QImage graphic(const QString &title,const QString &subtitle,const QColor &accent,QSize size={960,540}){
    QImage image(size,QImage::Format_ARGB32_Premultiplied);image.fill(QColor("#081222"));QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    p.scale(size.width()/960.,size.height()/540.);QLinearGradient gradient(0,0,960,540);gradient.setColorAt(0,QColor("#111a2d"));gradient.setColorAt(1,accent.darker(200));p.fillRect(QRect(0,0,960,540),gradient);
    p.setPen(Qt::NoPen);p.setBrush(accent);p.drawRoundedRect(QRect(56,62,105,8),4,4);p.setBrush(QColor(255,255,255,12));p.drawEllipse(QPoint(880,90),230,230);p.drawEllipse(QPoint(890,100),150,150);
    label(p,{56,128,850,85},title,38);label(p,{58,225,840,78},subtitle,19,QColor("#c5d4e9"),QFont::Normal);
    label(p,{58,430,830,52},"JPFACHINA  /  DEMONSTRAÇÃO",14,QColor("#9eb5d3"));return image;
}
static QString saveImage(const QString &root,const QString &name,const QImage &image){const auto path=root+"/"+name+".png";require(image.save(path),"Could not save demo graphic");return path;}
int main(int argc,char **argv){
    QApplication app(argc,argv);app.setFont(QFont("Segoe UI",10));QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    try{
        const auto args=app.arguments();auto argument=[&](const QString &key){const int index=args.indexOf(key);require(index>=0&&index+1<args.size(),"Missing argument: "+key);return args[index+1];};
        const auto output=QDir(argument("--output")).absolutePath(),repo=QDir(argument("--repo")).absolutePath();require(QDir().mkpath(output),"Cannot create image folder");
        QTemporaryDir temp;require(temp.isValid(),"Cannot create isolated demo data");jp::Store store(temp.path()+"/library");store.open();const auto project=store.snapshot()["active"].toString();store.renameProject(project,"Conferência JPFachina");
        auto import=[&](const QString &file,const QString &category,const QString &name){const auto id=store.importFile(project,file,category);store.updateMedia(project,id,{{"name",name}});return id;};
        auto plate=[&](const QString &file,const QString &category,const QString &title,const QString &description,const QColor &accent){return import(saveImage(temp.path(),file,graphic(title,description,accent)),category,title);};
        const auto logo=import(repo+"/assets/branding/jpfachina-sync-original.png","Logos","JPFachina · principal");
        const auto selected=plate("logo-studio","Logos","ESTÚDIO JP","Versão para entrevistas",QColor("#009cff"));plate("logo-live","Logos","AO VIVO","Identidade da transmissão",QColor("#6047d8"));plate("logo-event","Logos","CONFERÊNCIA","Edição especial",QColor("#009bab"));
        QImage lower(1800,240,QImage::Format_ARGB32_Premultiplied);lower.fill(Qt::transparent);{QPainter p(&lower);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(QColor("#102440"));p.drawRoundedRect(QRect(0,0,1800,240),22,22);p.setBrush(QColor("#009cff"));p.drawRoundedRect(QRect(0,0,18,240),8,8);label(p,{65,25,1650,105},"CONFERÊNCIA JPFACHINA",50);label(p,{68,132,1650,70},"Programação especial  •  Transmissão ao vivo",27,QColor("#c3d9f5"),QFont::Normal);}
        const auto banner=import(saveImage(temp.path(),"banner-principal",lower),"Banner","Conferência · identificação");plate("banner-agenda","Banner","PROGRAMAÇÃO","Confira os próximos encontros",QColor("#1462dd"));plate("banner-social","Banner","FIQUE POR PERTO","Acompanhe a nossa programação",QColor("#7049cc"));
        QImage qr(600,760,QImage::Format_ARGB32_Premultiplied);qr.fill(Qt::white);{QPainter p(&qr);p.drawImage(QRect(25,20,550,550),QImage(repo+"/assets/examples/qr-example.png"));label(p,{55,580,510,75},"PROGRAMAÇÃO",24,QColor("#102440"));label(p,{55,660,510,55},"example.org",19,QColor("#3c597c"),QFont::Normal);}
        const auto qrId=import(saveImage(temp.path(),"qr-programacao",qr),"QR-Codes","Programação · exemplo");import(saveImage(temp.path(),"qr-site",qr),"QR-Codes","Site · exemplo");import(saveImage(temp.path(),"qr-agenda",qr),"QR-Codes","Agenda · exemplo");
        for(const auto &category:QStringList{"Vinhetas","Abertura"}){plate(category+"-1",category,category=="Vinhetas"?"NOSSO ENCONTRO":"VAMOS COMEÇAR","Arte demonstrativa do evento",QColor("#225be4"));plate(category+"-2",category,"BEM-VINDOS","Conferência JPFachina",QColor("#583bc9"));plate(category+"-3",category,"EM INSTANTES","Prepare-se para a programação",QColor("#009aaa"));}
        QStringList videos;for(int i=0;i<3;++i)videos<<import(repo+"/tests/fixtures/sample.mp4","Vídeos",QStringList{"Clipe de teste · loop","Clipe de teste · intervalo","Clipe de teste · encerramento"}[i]);
        const QSize frame(1920,1080);store.saveCategoryLayouts(project,{{"Logos",QJsonObject{{"x",1570},{"y",65},{"width",275},{"height",140}}},{"Banner",QJsonObject{{"x",85},{"y",865},{"width",1260},{"height",168}}},{"QR-Codes",QJsonObject{{"x",1600},{"y",725},{"width",235},{"height",298}}}},frame);
        store.duplicateProject(project,"Live de domingo");store.createProject("Estúdio · entrevistas");store.selectProject(project);
        jp::Downloader downloader(store);jp::Controller controller(store,downloader);jp::ObsConnections connections(controller);jp::ObsWebSocket inspector;
        const QJsonObject config{{"mode","websocket"},{"host","127.0.0.1"},{"port",argument("--port").toInt()},{"tls",false}};const auto password=argument("--password");controller.connectionRequested(config,password);inspector.open(config,password);
        require(waitFor([&]{return controller.online&&inspector.ready()&&!controller.obsScenes.isEmpty();}),"Isolated OBS did not connect");const auto scene=controller.obsScenes.first().toObject()["uuid"].toString();
        QImage background(1920,1080,QImage::Format_ARGB32_Premultiplied);{QPainter p(&background);QLinearGradient g(0,0,1920,1080);g.setColorAt(0,QColor("#061426"));g.setColorAt(1,QColor("#123466"));p.fillRect(background.rect(),g);p.setPen(QPen(QColor("#1c4270"),2));for(int x=-800;x<2300;x+=120)p.drawLine(x,1080,x+950,0);label(p,{110,180,1350,110},"JPFACHINA",65);label(p,{112,325,1370,115},"CONFERÊNCIA",66);label(p,{116,475,1250,75},"Conteúdo, conexão e transmissão.",28,QColor("#bad5f6"),QFont::Normal);label(p,{116,630,1260,55},"CENÁRIO DEMONSTRATIVO  /  SAÍDA REAL DO OBS",17,QColor("#8bafd9"));}
        rpc(inspector,"CreateInput",{{"sceneUuid",scene},{"inputName","Cenário demonstrativo"},{"inputKind","image_source"},{"inputSettings",QJsonObject{{"file",saveImage(temp.path(),"cenario",background)}}},{"sceneItemEnabled",true}});
        controller.sceneAction(scene,"associate",true,"");require(waitFor([&]{return store.project(project)["obsBindings"].toArray().size()==6;}),"Association was not confirmed");
        require(waitFor([&]{for(const auto &row:controller.obsScenes)for(const auto &source:row.toObject()["sources"].toArray())if(source.toObject()["project"].toString()==project)return true;return false;}),"Managed source list was not refreshed");settle(300);
        for(const auto &id:QStringList{logo,banner,qrId}){controller.select(id);controller.take();require(waitFor([&]{return controller.onAir(id);}),"Demo Take was not confirmed by OBS");}
        const auto source=store.project(project)["obsBindings"].toArray().first().toObject()["sourceUuid"].toString();
        auto screenshot=[&](const QString &uuid){const auto data=rpc(inspector,"GetSourceScreenshot",{{"sourceUuid",uuid},{"imageFormat","png"},{"imageWidth",1920},{"imageHeight",1080}})["imageData"].toString();require(data.startsWith("data:image/png;base64,"),"OBS returned no screenshot");return QImage::fromData(QByteArray::fromBase64(data.mid(data.indexOf(',')+1).toLatin1()));};
        require(screenshot(scene).save(output+"/03-saida-obs.png"),"Could not save real program frame");
        jp::OperatorPanel panel(controller);panel.setAttribute(Qt::WA_DontShowOnScreen);panel.resize(1536,864);panel.show();controller.select(selected);settle(600);
        require(panel.grab().save(output+"/01-painel-aba1.png"),"Could not capture tab one");
        auto dialog=[&](const char *button,const char *name,const QString &file,bool sceneTab=false){bool saved=false;QTimer::singleShot(450,&panel,[&]{auto *widget=panel.findChild<QDialog*>(name);if(!widget){QApplication::closeAllWindows();return;}if(sceneTab){auto *tabs=widget->findChild<QTabWidget*>();if(tabs)tabs->setCurrentIndex(1);}settle(200);saved=widget->grab().save(output+"/"+file);widget->reject();});auto *action=panel.findChild<QPushButton*>(button);require(action,"Missing production UI button");action->click();require(saved,"Could not capture production dialog");};
        dialog("jpScenes","jpProjectsDialog","05-projetos.png");dialog("jpLink","jpLinkDialog","06-link-obs.png",true);
        auto *editor=panel.findChild<jp::PositionDialog*>();require(editor,"Position editor not found");editor->openCategory("QR-Codes");settle(500);require(editor->grab().save(output+"/04-editor-posicao.png"),"Could not capture position editor");editor->reject();
        panel.findChild<QPushButton*>("jpTab2")->click();controller.select(videos.first());controller.take();require(waitFor([&]{return controller.onAir(videos.first());}),"Video not rendered");const auto thumbnail=screenshot(source).scaled(640,360,Qt::KeepAspectRatio,Qt::SmoothTransformation);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);thumbnail.save(&buffer,"PNG");for(const auto &id:videos)store.saveThumbnail(project,id,bytes);controller.select(videos[1]);settle(500);require(panel.grab().save(output+"/02-painel-aba2.png"),"Could not capture tab two");
        store.clearLive(project);require(waitFor([&]{return controller.confirmed.isEmpty();}),"Demo output did not clear");inspector.close();controller.link(false);
        QTextStream(stdout)<<"PASS six real Qt/OBS screenshots; isolated data, confirmed tally and video playback.\n";return 0;
    }catch(const std::exception &error){QTextStream(stderr)<<"Gallery: "<<error.what()<<"\n";return 1;}
}
