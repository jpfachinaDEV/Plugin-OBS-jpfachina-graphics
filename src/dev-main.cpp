#include "server.hpp"
#include "downloader.hpp"
#include "operator-panel.hpp"
#include "operator-window.hpp"
#include "obs-websocket.hpp"
#include <QVBoxLayout>
#include <QApplication>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

int main(int argc,char **argv){
    QApplication app(argc,argv);app.setApplicationName("JP Fachina Graphics Dev");
    const auto args=app.arguments();const int rootIndex=args.indexOf("--data");
    const QString root=rootIndex>=0&&rootIndex+1<args.size()?args[rootIndex+1]:QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    try{
        if(args.contains("--native-ui")||(!args.contains("--legacy-ui")&&!args.contains("--no-browser"))){
            jp::Store store(root);store.open();jp::Downloader downloader(store);jp::Controller controller(store,downloader);jp::ObsConnections connections(controller);
            QWidget dock;dock.setWindowTitle("JP Fachina Graphics · Desenvolvimento Qt");auto *layout=new QVBoxLayout(&dock);auto *panel=new jp::OperatorPanel(controller);layout->addWidget(panel);
            jp::OperatorWindow operation(panel,&dock,root);
            panel->setDisplayMode("dock");QObject::connect(&operation,&jp::OperatorWindow::modeChanged,panel,&jp::OperatorPanel::setDisplayMode);
            QObject::connect(panel,&jp::OperatorPanel::windowRequested,&operation,&jp::OperatorWindow::detach);
            QObject::connect(panel,&jp::OperatorPanel::fullscreenRequested,&operation,&jp::OperatorWindow::toggleFullscreen);
            QObject::connect(panel,&jp::OperatorPanel::dockRequested,&operation,&jp::OperatorWindow::dock);
            QObject::connect(panel,&jp::OperatorPanel::monitorRequested,&operation,[&]{operation.settings(panel);});
            QObject::connect(&operation,&jp::OperatorWindow::warning,&controller,[&](const QString &m){controller.notify(m,true);});
            dock.resize(1366,768);dock.show();operation.restoreStartup();return app.exec();
        }
        jp::Store store(root);store.open();jp::Server server(store);server.start();jp::Downloader downloader(store);
        QObject::connect(&server,&jp::Server::downloadRequested,&downloader,&jp::Downloader::fetch);
        QObject::connect(&downloader,&jp::Downloader::completed,&server,&jp::Server::notice);
        QObject::connect(&server,&jp::Server::nativeAction,&server,[&](const QString &a,const QString &pid,const QString &cat){
            try{
                if(a=="native.import"){for(const auto &file:QFileDialog::getOpenFileNames(nullptr,"Adicionar mídias",{},"Mídias (*.png *.jpg *.jpeg *.gif *.webp *.webm *.mp4)"))store.importFile(pid,file,cat);}
                else if(a=="native.folder")QDesktopServices::openUrl(QUrl::fromLocalFile(root));
                else if(a=="native.source")server.notice("Modo de desenvolvimento: copie a URL do overlay para uma fonte de navegador 1920 × 1080 no OBS.");
                else if(a=="native.about")QMessageBox::information(nullptr,"JP Fachina Graphics","Modo de desenvolvimento. A integração com cenas só está disponível no plugin OBS.");
            }catch(const std::exception &e){server.notice(QString::fromUtf8(e.what()),true);}
        });
        QTextStream(stdout)<<"JP Fachina Graphics development server\n"<<server.panelUrl()<<"\n";
        if(!args.contains("--no-browser"))QDesktopServices::openUrl(QUrl(server.panelUrl()));
        QObject::connect(&app,&QCoreApplication::aboutToQuit,&server,&jp::Server::stop);
        return app.exec();
    }catch(const std::exception &e){QTextStream(stderr)<<e.what()<<"\n";return 1;}
}
