#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/bmem.h>
#include "server.hpp"
#include "operator-panel.hpp"
#include "operator-window.hpp"
#include "obs-native.hpp"
#include "obs-websocket.hpp"
#include <QApplication>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QJsonDocument>
#include <QBuffer>
#include <QSet>
#include <memory>
#include <vector>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_name(void){return "JP Fachina Graphics";}
MODULE_EXPORT const char *obs_module_description(void){return "Central nativa de grafismos, operador em tela cheia e composição privada.";}
MODULE_EXPORT const char *obs_module_author(void){return "JP Fachina Graphics";}

namespace {
constexpr auto dockId="jp-fachina-graphics-dock";
using Source=std::unique_ptr<obs_source_t,decltype(&obs_source_release)>;
Source ref(obs_source_t *source){return Source(source,&obs_source_release);}
bool legacySource(obs_source_t *source){if(strcmp(obs_source_get_id(source),"browser_source"))return false;auto *settings=obs_source_get_settings(source);const bool owned=obs_data_get_string(settings,"jp_fachina_project")[0]!=0;obs_data_release(settings);return owned;}
void visitScene(obs_scene_t *scene,QSet<obs_scene_t*> &visited,const std::function<void(obs_sceneitem_t*)> &visitor){
    if(!scene||visited.contains(scene))return;visited.insert(scene);
    struct Scan{QSet<obs_scene_t*> &visited;const std::function<void(obs_sceneitem_t*)> &visitor;}scan{visited,visitor};
    obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t *item,void *data){auto &scan=*static_cast<Scan*>(data);scan.visitor(item);auto *nested=obs_sceneitem_group_get_scene(item);if(!nested)nested=obs_scene_from_source(obs_sceneitem_get_source(item));visitScene(nested,scan.visited,scan.visitor);return true;},&scan);
}
struct Runtime {
    std::unique_ptr<jp::Store> store;
    std::unique_ptr<jp::Downloader> downloader;
    std::unique_ptr<jp::Controller> controller;
    std::unique_ptr<jp::OperatorWindow> operation;
    std::unique_ptr<jp::ObsConnections> connections;
    std::unique_ptr<jp::Server> legacy;
    Source preview=ref(nullptr);
    QPointer<QWidget> container;
    QPointer<jp::OperatorPanel> panel;
    QTimer updates,poll;
    bool dockRegistered=false,stopping=false;
    QSet<QString> thumbnailPending;
    QSet<QString> completedMedia;
    QMap<QString,QJsonObject> sentStates;
    quint64 thumbnailSerial=0,observationSequence=0;
    ~Runtime(){
        stopping=true;updates.stop();poll.stop();obs_queue_task(OBS_TASK_GRAPHICS,[](void*){},nullptr,true);if(controller)controller->setObsState(false,false,false,"OBS encerrando");
        connections.reset();operation.reset();if(panel)delete panel.data();preview.reset();
        if(dockRegistered)obs_frontend_remove_dock(dockId);else if(container)delete container.data();
        legacy.reset();controller.reset();downloader.reset();store.reset();
    }
    void start(){
        char *config=obs_module_config_path("");if(!config)throw jp::Error("Diretório de configuração OBS indisponível.");const auto root=QString::fromUtf8(config);bfree(config);
        char *qtPlugins=obs_module_file("qt-plugins");if(qtPlugins){QCoreApplication::addLibraryPath(QString::fromUtf8(qtPlugins));bfree(qtPlugins);}
        store=std::make_unique<jp::Store>(root);store->open();downloader=std::make_unique<jp::Downloader>(*store);controller=std::make_unique<jp::Controller>(*store,*downloader);
        controller->preflightTake=[this]{
            if(controller->project()["obsLink"].toObject()["mode"].toString()=="websocket")return;
            bool associated=false;const auto cat=jp::panelCategory(controller->selected()["category"].toString(),controller->selected()["mime"].toString());
            for(const auto &v:sceneSnapshot())for(const auto &s:v.toObject()["sources"].toArray()){const auto source=s.toObject();if(!source["external"].toBool()&&source["project"].toString()==controller->projectId()&&(source["category"].toString().isEmpty()||source["category"].toString()==cat))associated=true;}
            if(!associated)throw jp::Error("Associe a categoria a uma cena pelo botão Link OBS antes do Take.");
        };
        controller->videoSize=[]{obs_video_info info{};if(obs_get_video_info(&info))return QSize(int(info.base_width),int(info.base_height));return QSize(1920,1080);};
        controller->reconnect=[this]{refreshOutputs();status();refreshScenes();};
        controller->scenes=[this]{return sceneSnapshot();};
        controller->scenesRefresh=[this]{refreshScenes();};
        controller->sceneAction=[this](const QString &target,const QString &action,bool enabled,const QString &category){
            auto source=ref(obs_get_source_by_uuid(target.toUtf8().constData()));if(!source)source=ref(obs_get_source_by_name(target.toUtf8().constData()));if(!source||!obs_scene_from_source(source.get()))throw jp::Error("Cena indisponível; atualize as associações em Link OBS.");
            if(action=="associate"){associate(controller->projectId(),target,category);refreshScenes();return;}
            if(enabled){bool overlap=false;QSet<obs_scene_t*> checked;visitScene(obs_scene_from_source(source.get()),checked,[&](obs_sceneitem_t *item){auto *s=obs_sceneitem_get_source(item);if(!obs_sceneitem_visible(item)||strcmp(obs_source_get_id(s),jp::nativeSourceId))return;auto *settings=obs_source_get_settings(s);const bool own=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==controller->projectId();const auto existing=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"));obs_data_release(settings);if(own&&existing!=category&&(existing.isEmpty()||category.isEmpty()))overlap=true;});if(overlap)throw jp::Error("Oculte primeiro a composição sobreposta, para evitar exibição duplicada.");}
            bool found=false;QSet<obs_scene_t*> visited;visitScene(obs_scene_from_source(source.get()),visited,[&](obs_sceneitem_t *item){auto *s=obs_sceneitem_get_source(item);if(strcmp(obs_source_get_id(s),jp::nativeSourceId))return;auto *settings=obs_source_get_settings(s);const bool own=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==controller->projectId()&&QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"))==category&&!obs_data_get_bool(settings,"jp_fachina_external");obs_data_release(settings);if(own){found=true;obs_sceneitem_set_visible(item,enabled);}});
            if(!found)throw jp::Error("Fonte gerenciada inexistente para esta categoria e cena. Associe em Link OBS.");obs_frontend_save();status();refreshScenes();
        };
        connections=std::make_unique<jp::ObsConnections>(*controller);
        auto *settings=obs_data_create();obs_data_set_string(settings,"state","{\"width\":1920,\"height\":1080,\"items\":[]}");preview=ref(obs_source_create_private(jp::nativeSourceId,"JP · PREVIEW ISOLADO",settings));obs_data_release(settings);
        if(!preview)throw jp::Error("Não foi possível criar o compositor de preview.");
        container=new QWidget();auto *layout=new QVBoxLayout(container);layout->setContentsMargins(0,0,0,0);
        auto *landing=new QWidget(container);auto *launcher=new QVBoxLayout(landing);auto *summary=new QLabel("Central de grafismos disponível na janela de operação.",landing);summary->setWordWrap(true);launcher->addWidget(summary);
        auto *returnButton=new QPushButton("Retornar ao dock",landing);auto *showButton=new QPushButton("Mostrar operador",landing);launcher->addWidget(returnButton);launcher->addWidget(showButton);layout->addWidget(landing);landing->hide();
        panel=new jp::OperatorPanel(*controller,container);layout->addWidget(panel);
        operation=std::make_unique<jp::OperatorWindow>(panel,container,root);
        panel->setDisplayMode("dock");QObject::connect(operation.get(),&jp::OperatorWindow::modeChanged,panel,&jp::OperatorPanel::setDisplayMode);
        QObject::connect(panel,&jp::OperatorPanel::windowRequested,operation.get(),&jp::OperatorWindow::detach);
        QObject::connect(panel,&jp::OperatorPanel::fullscreenRequested,operation.get(),&jp::OperatorWindow::toggleFullscreen);
        QObject::connect(panel,&jp::OperatorPanel::dockRequested,operation.get(),&jp::OperatorWindow::dock);
        QObject::connect(panel,&jp::OperatorPanel::monitorRequested,operation.get(),[this]{operation->settings(panel);});
        QObject::connect(operation.get(),&jp::OperatorWindow::warning,controller.get(),[this](const QString &message){controller->notify(message,true);});
        QObject::connect(operation.get(),&jp::OperatorWindow::modeChanged,landing,[landing](const QString &mode){landing->setVisible(mode!="dock");});
        QObject::connect(returnButton,&QPushButton::clicked,operation.get(),&jp::OperatorWindow::dock);
        QObject::connect(showButton,&QPushButton::clicked,operation.get(),[this]{if(operation->mode()=="fullscreen"){operation->raise();operation->activateWindow();}else operation->detach();});
        QObject::connect(controller.get(),&jp::Controller::changed,landing,[this,summary]{summary->setText(controller->obsStatus+"\nProjeto: "+controller->project()["name"].toString());});
        dockRegistered=obs_frontend_add_dock_by_id(dockId,"JP Fachina Graphics",container);if(!dockRegistered)throw jp::Error("Não foi possível registrar o dock.");
        updates.setSingleShot(true);updates.setInterval(40);QObject::connect(&updates,&QTimer::timeout,controller.get(),[this]{refreshOutputs();});
        QObject::connect(store.get(),&jp::Store::changed,controller.get(),[this]{updates.start();});QObject::connect(controller.get(),&jp::Controller::selectionChanged,controller.get(),[this]{prepareThumbnail();});
        poll.setInterval(250);QObject::connect(&poll,&QTimer::timeout,controller.get(),[this]{status();});poll.start();refreshOutputs();status();refreshScenes();operation->restoreStartup();
        blog(LOG_INFO,"[JP Fachina Graphics] Native Qt dock and isolated compositor loaded; operator ready.");
    }
    std::vector<Source> ownSources(const char *type){
        struct Context{const char *type;std::vector<Source> sources;} context{type,{}};
        obs_enum_sources([](void *data,obs_source_t *source){auto &c=*static_cast<Context*>(data);if(strcmp(obs_source_get_id(source),c.type))return true;
            auto *s=obs_source_get_settings(source);const bool owned=obs_data_get_string(s,"jp_fachina_project")[0]!=0;obs_data_release(s);if(owned)c.sources.emplace_back(ref(obs_source_get_ref(source)));return true;},&context);return std::move(context.sources);
    }
    bool legacyInScenes(){
        bool found=false;
        obs_enum_sources([](void *data,obs_source_t *source){QSet<obs_scene_t*> visited;visitScene(obs_scene_from_source(source),visited,[data](obs_sceneitem_t *item){if(legacySource(obs_sceneitem_get_source(item)))*static_cast<bool*>(data)=true;});return !*static_cast<bool*>(data);
        },&found);return found;
    }
    void refreshOutputs(){
        if(stopping)return;
        try{
            for(auto &source:ownSources(jp::nativeSourceId)){
                auto *settings=obs_source_get_settings(source.get());const bool external=obs_data_get_bool(settings,"jp_fachina_external");const auto pid=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project")),category=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"));obs_data_release(settings);if(external)continue;
                QJsonObject state;try{auto project=store->project(pid);if(!category.isEmpty()){QJsonArray media;for(const auto &v:project["media"].toArray()){const auto m=v.toObject();if(jp::panelCategory(m["category"].toString(),m["mime"].toString())==category)media.append(m);}project["media"]=media;}state=jp::nativeState(*store,project);}catch(const jp::Error &){state={{"width",1920},{"height",1080},{"items",QJsonArray{}}};}
                const auto uuid=QString::fromUtf8(obs_source_get_uuid(source.get()));if(sentStates.value(uuid)!=state){jp::updateNativeSource(source.get(),state);sentStates[uuid]=state;}
            }
            const bool hasLegacy=legacyInScenes();
            if(hasLegacy&&!legacy){legacy=std::make_unique<jp::Server>(*store);try{legacy->start();QObject::connect(legacy.get(),&jp::Server::downloadRequested,downloader.get(),&jp::Downloader::fetch);controller->notify("Compatibilidade ativa para fontes Browser Source antigas. Associe cada cena para migrar à saída nativa.");}catch(...){legacy.reset();throw;}}
            else if(!hasLegacy&&legacy){legacy.reset();controller->notify("Migração concluída: operação nativa sem servidor HTTP.");}
        }catch(const std::exception &e){controller->notify(QString::fromUtf8(e.what()),true);}
    }
    void status(){
        QSet<obs_source_t*> programSources;auto output=ref(obs_get_output_source(0));if(output){programSources.insert(output.get());obs_source_enum_active_tree(output.get(),[](obs_source_t*,obs_source_t *child,void *data){static_cast<QSet<obs_source_t*>*>(data)->insert(child);},&programSources);}
        bool visible=false;QMap<QString,QSet<QString>> confirmed;QMap<QString,QMap<QString,int>> generations;
        for(auto &source:ownSources(jp::nativeSourceId)){
            auto *settings=obs_source_get_settings(source.get());const auto pid=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"));const bool external=obs_data_get_bool(settings,"jp_fachina_external");obs_data_release(settings);
            auto observed=jp::nativeObserved(source.get());const bool active=programSources.contains(source.get());observed["program"]=active;observed["sequence"]=double(++observationSequence);
            // WebSocket clients read this post-render snapshot; requested state never serves as tally.
            auto *snapshot=obs_data_create();const auto bytes=QJsonDocument(observed).toJson(QJsonDocument::Compact);obs_data_set_string(snapshot,"jp_fachina_observed",bytes.constData());obs_source_update(source.get(),snapshot);obs_data_release(snapshot);
            if(active){if(pid==controller->projectId())visible=true;for(const auto &v:observed["items"].toArray()){const auto item=v.toObject();confirmed[pid].insert(item["id"].toString());generations[pid][item["id"].toString()]=item["generation"].toInt();}}
            if(external)continue;
            for(const auto &v:observed["ended"].toArray()){const auto end=v.toObject();const auto key=pid+"/"+end["id"].toString()+"/"+QString::number(end["generation"].toInt());if(completedMedia.contains(key))continue;if(completedMedia.size()>2000)completedMedia.clear();completedMedia.insert(key);try{for(const auto &m:store->project(pid)["media"].toArray()){const auto item=m.toObject();if(item["id"]==end["id"]&&item["generation"]==end["generation"]&&item["live"].isObject()&&item["live"].toObject()["endBehavior"].toString("hold")=="out")store->setLive(pid,item["id"].toString(),false);}}catch(const jp::Error &){} }
        }
        controller->confirmProvider("native",confirmed,generations);
        if(controller->linked&&controller->project()["obsLink"].toObject()["mode"].toString("native")=="native")controller->setObsState(true,obs_frontend_streaming_active(),obs_frontend_recording_active(),QString("OBS integrado · %1%2").arg(visible?"composição ativa no programa":"projeto fora do programa",legacy?" · fontes antigas":""));
    }
    QJsonArray sceneSnapshot(){
        QJsonArray rows;obs_frontend_source_list list{};obs_frontend_get_scenes(&list);
        for(size_t i=0;i<list.sources.num;++i){auto *scene=list.sources.array[i];QJsonArray sources;QSet<obs_scene_t*> visited;
            visitScene(obs_scene_from_source(scene),visited,[&](obs_sceneitem_t *item){auto *source=obs_sceneitem_get_source(item);if(strcmp(obs_source_get_id(source),jp::nativeSourceId))return;auto *settings=obs_source_get_settings(source);sources.append(QJsonObject{{"name",QString::fromUtf8(obs_source_get_name(source))},{"uuid",QString::fromUtf8(obs_source_get_uuid(source))},{"project",QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))},{"category",QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"))},{"external",obs_data_get_bool(settings,"jp_fachina_external")},{"visible",obs_sceneitem_visible(item)}});obs_data_release(settings);});
            rows.append(QJsonObject{{"name",QString::fromUtf8(obs_source_get_name(scene))},{"uuid",QString::fromUtf8(obs_source_get_uuid(scene))},{"sources",sources}});
        }obs_frontend_source_list_free(&list);return rows;
    }
    void refreshScenes(){
        const auto rows=sceneSnapshot();if(controller->project()["obsLink"].toObject()["mode"].toString("native")=="native"){controller->obsScenes=rows;emit controller->obsScenesChanged();}
        for(const auto &v:store->snapshot()["projects"].toArray()){const auto p=v.toObject();if(p["obsLink"].toObject()["mode"].toString("native")!="native")continue;QJsonArray bindings;for(const auto &s:rows){const auto scene=s.toObject();for(const auto &e:scene["sources"].toArray()){const auto source=e.toObject();if(source["project"]!=p["id"]||source["external"].toBool())continue;const auto category=source["category"].toString();for(const auto &cat:category.isEmpty()?jp::panelCategories():QStringList{category})bindings.append(QJsonObject{{"sceneUuid",scene["uuid"]},{"sceneName",scene["name"]},{"sourceUuid",source["uuid"]},{"sourceName",source["name"]},{"category",cat}});}}
            for(const auto &old:p["obsBindings"].toArray()){auto b=old.toObject();bool present=false;for(const auto &bindingValue:bindings)if(bindingValue.toObject()["sceneUuid"]==b["sceneUuid"]&&bindingValue.toObject()["sourceUuid"]==b["sourceUuid"]&&bindingValue.toObject()["category"]==b["category"]){present=true;break;}if(!present){b["missing"]=true;bindings.append(b);}}
            if(bindings!=p["obsBindings"].toArray())store->obsBindings(p["id"].toString(),bindings);
        }
    }
    void prepareThumbnail(){
        const auto serial=++thumbnailSerial;const auto selected=controller->selected();if(!selected["mime"].toString().startsWith("video/")||selected.contains("thumbnail")){jp::updateNativeSource(preview.get(),{{"items",QJsonArray{}}});return;}
        const auto pid=controller->projectId(),id=controller->selectedId;jp::updateNativeSource(preview.get(),jp::nativeState(*store,controller->project(),id,selected["draft"].toObject()));
        QTimer::singleShot(700,controller.get(),[this,serial,pid,id]{if(stopping||serial!=thumbnailSerial)return;jp::nativeThumbnailAsync(preview.get(),controller.get(),[this,serial,pid,id](QImage image){if(stopping||serial!=thumbnailSerial)return;jp::updateNativeSource(preview.get(),{{"items",QJsonArray{}}});if(image.isNull())return;QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);if(image.save(&buffer,"PNG"))try{store->saveThumbnail(pid,id,bytes);}catch(const std::exception &e){controller->notify(QString::fromUtf8(e.what()),true);}});});
    }
    void associate(const QString &pid,const QString &target={},const QString &category={}){
        const auto project=store->project(pid);auto sceneSource=ref(target.isEmpty()?obs_frontend_get_current_scene():obs_get_source_by_uuid(target.toUtf8().constData()));if(!sceneSource&&!target.isEmpty())sceneSource=ref(obs_get_source_by_name(target.toUtf8().constData()));if(!sceneSource)throw jp::Error("Selecione uma cena no OBS.");auto *scene=obs_scene_from_source(sceneSource.get());if(!scene)throw jp::Error("Cena indisponível.");
        const auto sceneName=QString::fromUtf8(obs_source_get_name(sceneSource.get()));
        QSet<obs_scene_t*> overlapVisited;bool overlaps=false;
        visitScene(scene,overlapVisited,[&](obs_sceneitem_t *item){if(!obs_sceneitem_visible(item))return;auto *s=obs_sceneitem_get_source(item);if(strcmp(obs_source_get_id(s),jp::nativeSourceId)&&!legacySource(s))return;auto *settings=obs_source_get_settings(s);const bool own=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==pid;const auto existing=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"));obs_data_release(settings);if(own&&existing!=category&&(existing.isEmpty()||category.isEmpty()))overlaps=true;});
        if(overlaps)throw jp::Error("Esta cena já possui uma composição que cobre a mesma categoria. Oculte esse vínculo em Link OBS antes de criar outro, para evitar exibição duplicada.");
        if(QMessageBox::question(panel,"Associar composição nativa",QString("Associar '%1' à cena '%2'?\n\nFontes antigas deste projeto nesta cena serão substituídas pela composição nativa, preservando posição e recorte. Câmeras e outras fontes permanecem intactas.\n\nFaça a migração fora de uma transmissão ao vivo.").arg(project["name"].toString(),sceneName),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
        Source output=ref(nullptr);QSet<obs_scene_t*> existingVisited;visitScene(scene,existingVisited,[&](obs_sceneitem_t *item){auto *s=obs_sceneitem_get_source(item);if(output||strcmp(obs_source_get_id(s),jp::nativeSourceId))return;auto *settings=obs_source_get_settings(s);const bool match=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==pid&&QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"))==category&&!obs_data_get_bool(settings,"jp_fachina_external");obs_data_release(settings);if(match)output=ref(obs_source_get_ref(s));});
        if(!output)for(auto &s:ownSources(jp::nativeSourceId)){auto *settings=obs_source_get_settings(s.get());const bool match=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==pid&&QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_category"))==category&&!obs_data_get_bool(settings,"jp_fachina_external");obs_data_release(settings);if(match){output=std::move(s);break;}}
        if(!output){const auto name="JP Graphics · "+project["name"].toString()+" · "+pid.left(8)+(category.isEmpty()?QString():" · "+category);auto collision=ref(obs_get_source_by_name(name.toUtf8().constData()));if(collision)throw jp::Error("Já existe uma fonte com esse nome. Renomeie o projeto.");
            auto *settings=obs_data_create();obs_data_set_string(settings,"jp_fachina_project",pid.toUtf8().constData());obs_data_set_string(settings,"jp_fachina_category",category.toUtf8().constData());auto filtered=project;if(!category.isEmpty()){QJsonArray media;for(const auto &v:project["media"].toArray())if(jp::panelCategory(v.toObject()["category"].toString(),v.toObject()["mime"].toString())==category)media.append(v);filtered["media"]=media;}const auto state=QJsonDocument(jp::nativeState(*store,filtered)).toJson(QJsonDocument::Compact);obs_data_set_string(settings,"state",state.constData());output=ref(obs_source_create(jp::nativeSourceId,name.toUtf8().constData(),settings,nullptr));obs_data_release(settings);if(!output)throw jp::Error("Falha ao criar fonte nativa.");}
        struct Migration{QString pid;obs_source_t *source;bool present=false;std::vector<obs_sceneitem_t*> old;bool failed=false;} migration{pid,output.get()};
        QSet<obs_scene_t*> visited;visitScene(scene,visited,[&migration,&category](obs_sceneitem_t *item){auto *s=obs_sceneitem_get_source(item);if(s==migration.source)migration.present=true;if(!category.isEmpty()||!legacySource(s))return;auto *settings=obs_source_get_settings(s);const bool own=QString::fromUtf8(obs_data_get_string(settings,"jp_fachina_project"))==migration.pid;obs_data_release(settings);if(own){obs_sceneitem_addref(item);migration.old.push_back(item);}});
        if(migration.present&&!migration.old.empty()){for(auto *old:migration.old)obs_sceneitem_release(old);throw jp::Error("Esta cena já contém saídas nativas e antigas do mesmo projeto. Confira a composição duplicada antes de migrar; nenhum item foi alterado.");}
        obs_scene_atomic_update(scene,[](void *data,obs_scene_t *current){auto &m=*static_cast<Migration*>(data);
            if(m.old.empty()){if(!m.present){auto *item=obs_scene_add(current,m.source);if(!item)m.failed=true;else{obs_video_info video{};if(obs_get_video_info(&video)){obs_transform_info info{};info.scale.x=1;info.scale.y=1;info.alignment=OBS_ALIGN_LEFT|OBS_ALIGN_TOP;info.bounds_type=OBS_BOUNDS_SCALE_INNER;info.bounds.x=float(video.base_width);info.bounds.y=float(video.base_height);obs_sceneitem_set_info2(item,&info);}}}return;}
            std::vector<obs_sceneitem_t*> created;
            for(auto *old:m.old){auto *item=obs_scene_add(obs_sceneitem_get_scene(old),m.source);if(!item){for(auto *entry:created)obs_sceneitem_remove(entry);m.failed=true;return;}created.push_back(item);obs_sceneitem_set_visible(item,false);
                obs_transform_info info{};obs_sceneitem_get_info2(old,&info);obs_sceneitem_set_info2(item,&info);obs_sceneitem_crop crop{};obs_sceneitem_get_crop(old,&crop);obs_sceneitem_set_crop(item,&crop);obs_sceneitem_set_locked(item,obs_sceneitem_locked(old));}
            for(size_t i=0;i<m.old.size();++i){auto *old=m.old[i];obs_sceneitem_set_order_position(created[i],obs_sceneitem_get_order_position(old));obs_sceneitem_set_visible(created[i],obs_sceneitem_visible(old));obs_sceneitem_remove(old);}
        },&migration);
        for(auto *old:migration.old)obs_sceneitem_release(old);if(migration.failed)throw jp::Error("Falha na associação. Itens antigos preservados.");
        obs_frontend_save();refreshOutputs();status();controller->notify("Composição nativa associada à cena '"+sceneName+"'. Fontes antigas sem itens podem ser removidas pelo OBS após conferência.");
    }
};
std::unique_ptr<Runtime> runtime;
void start(){if(runtime)return;try{auto r=std::make_unique<Runtime>();r->start();runtime=std::move(r);}catch(const std::exception &e){blog(LOG_ERROR,"[JP Fachina Graphics] %s",e.what());QMessageBox::critical(static_cast<QWidget*>(obs_frontend_get_main_window()),"JP Fachina Graphics",QString::fromUtf8(e.what()));}}
void event(enum obs_frontend_event e,void*){
    if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)start();
    else if(e==OBS_FRONTEND_EVENT_EXIT||e==OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN)runtime.reset();
    else if(runtime&&(e==OBS_FRONTEND_EVENT_SCENE_CHANGED||e==OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED||e==OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED)){runtime->updates.start();runtime->status();runtime->refreshScenes();}
}
}
bool obs_module_load(void){jp::registerNativeSource();obs_frontend_add_event_callback(event,nullptr);return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(event,nullptr);runtime.reset();}
