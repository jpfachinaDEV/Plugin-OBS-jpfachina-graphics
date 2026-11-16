#include "obs-native.hpp"
#include "title-image.hpp"
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/matrix4.h>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QMutex>
#include <QMutexLocker>
#include <QEvent>
#include <QWindow>
#include <QPointer>
#include <QMetaObject>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>

namespace jp {
namespace {
uint32_t alignment(const QString &anchor){uint32_t result=0;if(anchor.contains("left"))result|=OBS_ALIGN_LEFT;if(anchor.contains("right"))result|=OBS_ALIGN_RIGHT;if(anchor.startsWith("top"))result|=OBS_ALIGN_TOP;if(anchor.startsWith("bottom"))result|=OBS_ALIGN_BOTTOM;return result;}
struct Element {
    obs_source_t *source=nullptr,*filter=nullptr;
    obs_sceneitem_t *item=nullptr;
    QJsonObject data;
    double elapsed=0;
    bool exiting=false;
    double alpha=-1;
    bool layoutDirty=true;
    bool endedHidden=false;
    uint32_t sourceWidth=0,sourceHeight=0;
    ~Element(){if(item)obs_sceneitem_remove(item);if(filter)obs_source_release(filter);if(source)obs_source_release(source);}
};
struct Output {
    obs_source_t *parent=nullptr;
    obs_scene_t *scene=nullptr;
    QMutex mutex;
    QJsonObject pending;
    QJsonObject observed;
    QByteArray lastState;
    quint64 appliedStates=0,createdSources=0;
    bool updated=false;
    std::atomic<uint32_t> width{1920},height{1080};
    std::map<QString,std::unique_ptr<Element>> elements;
    explicit Output(obs_source_t *source):parent(source){scene=obs_scene_create_private("JP · composição privada");if(scene)obs_source_add_active_child(parent,obs_scene_get_source(scene));
        proc_handler_add(obs_source_get_proc_handler(parent),"void jp_observed(out string json)",[](void *data,calldata_t *args){auto &o=*static_cast<Output*>(data);QMutexLocker lock(&o.mutex);const auto json=QJsonDocument(o.observed).toJson(QJsonDocument::Compact);calldata_set_string(args,"json",json.constData());},this);
    }
    ~Output(){if(scene)obs_source_remove_active_child(parent,obs_scene_get_source(scene));elements.clear();if(scene)obs_scene_release(scene);}
    void update(obs_data_t *settings){const QByteArray state=obs_data_get_string(settings,"state");QMutexLocker lock(&mutex);if(state==lastState)return;lastState=state;pending=QJsonDocument::fromJson(state).object();updated=true;width.store(uint32_t(qBound(1,pending["width"].toInt(1920),7680)));height.store(uint32_t(qBound(1,pending["height"].toInt(1080),4320)));}
    std::unique_ptr<Element> create(const QJsonObject &data){
        auto e=std::make_unique<Element>();e->data=data;const auto path=data["path"].toString();
        if(path.isEmpty()||!QFileInfo::exists(path)){blog(LOG_WARNING,"[JP Fachina Graphics] Missing cached media for element %s",qPrintable(data["id"].toString()));return {};}
        auto *settings=obs_data_create();const bool video=data["mime"].toString().startsWith("video/");
        if(video){obs_data_set_string(settings,"local_file",path.toUtf8().constData());obs_data_set_bool(settings,"is_local_file",true);obs_data_set_bool(settings,"looping",data["layout"].toObject()["loop"].toBool(true));obs_data_set_bool(settings,"clear_on_media_end",false);obs_data_set_bool(settings,"restart_on_activate",false);obs_data_set_bool(settings,"close_when_inactive",false);obs_data_set_bool(settings,"hw_decode",false);}
        else{obs_data_set_string(settings,"file",path.toUtf8().constData());obs_data_set_bool(settings,"unload",false);}
        e->source=obs_source_create_private(video?"ffmpeg_source":"image_source",data["name"].toString().toUtf8().constData(),settings);obs_data_release(settings);
        if(!e->source){blog(LOG_ERROR,"[JP Fachina Graphics] OBS media source unavailable");return {};}
        obs_source_set_muted(e->source,true);
        auto *filterSettings=obs_data_create();obs_data_set_double(filterSettings,"opacity",1.0);e->filter=obs_source_create_private("color_filter_v2","JP · opacidade privada",filterSettings);obs_data_release(filterSettings);
        if(e->filter)obs_source_filter_add(e->source,e->filter);
        else{blog(LOG_ERROR,"[JP Fachina Graphics] Required OBS color filter unavailable");return {};}
        e->item=obs_scene_add(scene,e->source);if(!e->item)return {};++createdSources;return e;
    }
    void apply(const QJsonObject &state){
        ++appliedStates;
        const auto items=state["items"].toArray();QSet<QString> keep;
        for(const auto &v:items){const auto data=v.toObject();const auto id=data["id"].toString();if(id.isEmpty())continue;keep.insert(id);
            auto found=elements.find(id);if(found!=elements.end()&&(found->second->data["path"]!=data["path"]||found->second->data["mime"]!=data["mime"])){elements.erase(found);found=elements.end();}
            if(found==elements.end()){auto element=create(data);if(!element)continue;elements.emplace(id,std::move(element));found=elements.find(id);}
            auto &e=*found->second;
            if(e.data["generation"]!=data["generation"]&&data["mime"].toString().startsWith("video/")){obs_source_media_restart(e.source);e.elapsed=0;}
            if(e.exiting){e.exiting=false;e.elapsed=0;}
            if(e.data["layout"].toObject()["loop"]!=data["layout"].toObject()["loop"]&&e.data["mime"].toString().startsWith("video/")){auto *settings=obs_data_create();obs_data_set_bool(settings,"looping",data["layout"].toObject()["loop"].toBool(true));obs_source_update(e.source,settings);obs_data_release(settings);}
            if(e.data["layout"]!=data["layout"])e.layoutDirty=true;e.data=data;
        }
        for(auto &pair:elements)if(!keep.contains(pair.first)&&!pair.second->exiting){pair.second->exiting=true;pair.second->elapsed=0;}
        QList<Element*> order;for(auto &pair:elements)order<<pair.second.get();std::stable_sort(order.begin(),order.end(),[](const Element *a,const Element *b){return a->data["layout"].toObject()["z"].toDouble()<b->data["layout"].toObject()["z"].toDouble();});for(int i=0;i<order.size();++i)obs_sceneitem_set_order_position(order[i]->item,i);
    }
    void tick(float seconds){
        QJsonObject state;bool applyPending=false;{QMutexLocker lock(&mutex);if(updated){state=pending;updated=false;applyPending=true;}}
        if(applyPending)apply(state);if(!scene)return;
        for(auto it=elements.begin();it!=elements.end();){auto &e=*it->second;const auto d=e.data["layout"].toObject();const auto transition=d["transition"].toString();const double duration=transition=="none"?0:d["duration"].toDouble(300)/1000;
            e.elapsed+=seconds;const double linear=duration<=0?1:std::min(1.0,e.elapsed/duration);const double eased=1-std::pow(1-linear,3);const double amount=e.exiting?1-eased:eased;
            if(e.exiting&&linear>=1){it=elements.erase(it);continue;}
            const bool ended=e.data["mime"].toString().startsWith("video/")&&!d["loop"].toBool(true)&&obs_source_media_get_state(e.source)==OBS_MEDIA_STATE_ENDED&&d["endBehavior"].toString("hold")=="out";
            if(e.endedHidden!=ended){e.endedHidden=ended;obs_sceneitem_set_visible(e.item,!ended);}
            const double opacity=ended?0:d["opacity"].toDouble(100)/100*(transition=="none"?1:amount);
            if(std::abs(e.alpha-opacity)>.001){auto *settings=obs_data_create();obs_data_set_double(settings,"opacity",opacity);obs_source_update(e.filter,settings);obs_data_release(settings);e.alpha=opacity;}
            const double w=d["width"].toDouble(1),h=d["height"].toDouble(1),scale=d["scale"].toDouble(1);
            const auto sw=obs_source_get_width(e.source),sh=obs_source_get_height(e.source);
            if(!e.layoutDirty&&linear>=1&&e.sourceWidth==sw&&e.sourceHeight==sh){++it;continue;}
            e.sourceWidth=sw;e.sourceHeight=sh;e.layoutDirty=linear<1;
            obs_sceneitem_crop crop{};crop.left=int(d["cropLeft"].toDouble()*sw/w);crop.right=int(d["cropRight"].toDouble()*sw/w);crop.top=int(d["cropTop"].toDouble()*sh/h);crop.bottom=int(d["cropBottom"].toDouble()*sh/h);obs_sceneitem_set_crop(e.item,&crop);
            obs_transform_info info{};info.pos.x=float(d["x"].toDouble());info.pos.y=float(d["y"].toDouble());info.rot=float(d["rotation"].toDouble());info.scale.x=1;info.scale.y=1;
            info.alignment=alignment(d["anchor"].toString("top-left"));info.bounds_type=OBS_BOUNDS_SCALE_INNER;info.bounds_alignment=alignment(d["alignment"].toString("center"));
            info.bounds.x=float((w-d["cropLeft"].toDouble()-d["cropRight"].toDouble())*scale);info.bounds.y=float((h-d["cropTop"].toDouble()-d["cropBottom"].toDouble())*scale);
            if(transition=="left")info.pos.x-=float((1-amount)*w*scale);if(transition=="right")info.pos.x+=float((1-amount)*w*scale);if(transition=="bottom")info.pos.y+=float((1-amount)*h*scale);
            if(transition=="scale"){info.bounds.x*=float(.8+.2*amount);info.bounds.y*=float(.8+.2*amount);}
            obs_sceneitem_set_info2(e.item,&info);++it;
        }
        QJsonArray actual,ended;
        for(const auto &pair:elements){const auto &e=*pair.second;
            if(obs_source_get_width(e.source)==0||obs_source_get_height(e.source)==0)continue;
            const auto layout=e.data["layout"].toObject();
            const bool finished=e.data["mime"].toString().startsWith("video/")&&!layout["loop"].toBool(true)&&obs_source_media_get_state(e.source)==OBS_MEDIA_STATE_ENDED;
            if(finished)ended.append(QJsonObject{{"id",pair.first},{"generation",e.data["generation"]}});
            if(layout["opacity"].toDouble(100)>0&&e.alpha>0)actual.append(QJsonObject{{"id",pair.first},{"generation",e.data["generation"]}});
        }
        {QMutexLocker lock(&mutex);observed={{"items",actual},{"ended",ended},{"appliedStates",double(appliedStates)},{"createdSources",double(createdSources)}};}
    }
};
}
void registerNativeSource(){
    obs_source_info info{};info.id=nativeSourceId;info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW;
    info.get_name=[](void*){return "JP Fachina · Composição de grafismos";};
    info.create=[](obs_data_t *settings,obs_source_t *source)->void*{try{auto output=std::make_unique<Output>(source);if(!output->scene)return nullptr;output->update(settings);return output.release();}catch(...){blog(LOG_ERROR,"[JP Fachina Graphics] Failed to create native compositor");return nullptr;}};
    info.destroy=[](void *data){delete static_cast<Output*>(data);};info.update=[](void *data,obs_data_t *settings){try{static_cast<Output*>(data)->update(settings);}catch(...){blog(LOG_ERROR,"[JP Fachina Graphics] Invalid composition update");}};
    info.get_width=[](void *data){return static_cast<Output*>(data)->width.load();};info.get_height=[](void *data){return static_cast<Output*>(data)->height.load();};
    info.video_tick=[](void *data,float seconds){try{static_cast<Output*>(data)->tick(seconds);}catch(...){blog(LOG_ERROR,"[JP Fachina Graphics] Composition update failed");}};
    info.video_render=[](void *data,gs_effect_t*){auto *output=static_cast<Output*>(data);if(output->scene)obs_source_video_render(obs_scene_get_source(output->scene));};
    info.enum_active_sources=[](void *data,obs_source_enum_proc_t callback,void *param){auto *output=static_cast<Output*>(data);if(output->scene)callback(output->parent,obs_scene_get_source(output->scene),param);};
    info.enum_all_sources=info.enum_active_sources;obs_register_source(&info);
}
QJsonObject nativeState(Store &store,const QJsonObject &project,const QString &selected,const QJsonObject &draft){return compositionState(store,project,selected,draft);}
void updateNativeSource(obs_source_t *source,const QJsonObject &state){auto *settings=obs_data_create();const auto bytes=QJsonDocument(state).toJson(QJsonDocument::Compact);obs_data_set_string(settings,"state",bytes.constData());obs_source_update(source,settings);obs_data_release(settings);}
QJsonObject nativeObserved(obs_source_t *source){calldata_t args;calldata_init(&args);QJsonObject result;if(proc_handler_call(obs_source_get_proc_handler(source),"jp_observed",&args))result=QJsonDocument::fromJson(calldata_string(&args,"json")).object();calldata_free(&args);return result;}
QImage nativeThumbnail(obs_source_t *preview){
    struct Request{obs_source_t *preview;QImage image;};Request request{preview,{}};
    obs_queue_task(OBS_TASK_GRAPHICS,[](void *param){auto &r=*static_cast<Request*>(param);obs_source_t *media=nullptr;
        obs_source_enum_active_sources(r.preview,[](obs_source_t*,obs_source_t *child,void *data){auto *scene=obs_scene_from_source(child);if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t *item,void *data){*static_cast<obs_source_t**>(data)=obs_source_get_ref(obs_sceneitem_get_source(item));return false;},data);},&media);
        if(!media)return;const auto size=QSize(int(obs_source_get_width(media)),int(obs_source_get_height(media))).scaled(320,180,Qt::KeepAspectRatio);if(size.isEmpty()){obs_source_release(media);return;}
        obs_enter_graphics();auto *texture=gs_texrender_create(GS_BGRA,GS_ZS_NONE);auto *stage=gs_stagesurface_create(uint32_t(size.width()),uint32_t(size.height()),GS_BGRA);gs_projection_push();gs_viewport_push();gs_matrix_push();
        if(texture&&stage&&gs_texrender_begin(texture,uint32_t(size.width()),uint32_t(size.height()))){vec4 color{};gs_clear(GS_CLEAR_COLOR,&color,0,0);gs_ortho(0,float(obs_source_get_width(media)),0,float(obs_source_get_height(media)),-100,100);gs_matrix_identity();obs_source_video_render(media);gs_texrender_end(texture);gs_stage_texture(stage,gs_texrender_get_texture(texture));uint8_t *pixels=nullptr;uint32_t stride=0;if(gs_stagesurface_map(stage,&pixels,&stride)){r.image=QImage(pixels,size.width(),size.height(),int(stride),QImage::Format_ARGB32_Premultiplied).copy();gs_stagesurface_unmap(stage);}}
        gs_matrix_pop();gs_viewport_pop();gs_projection_pop();if(stage)gs_stagesurface_destroy(stage);if(texture)gs_texrender_destroy(texture);obs_leave_graphics();obs_source_release(media);
    },&request,true);return request.image;
}
void nativeThumbnailAsync(obs_source_t *preview,QObject *context,std::function<void(QImage)> completed){
    struct Request{obs_source_t *source;QPointer<QObject> owner;std::function<void(QImage)> completed;};
    auto *request=new Request{obs_source_get_ref(preview),context,std::move(completed)};
    obs_queue_task(OBS_TASK_GRAPHICS,[](void *data){std::unique_ptr<Request> request(static_cast<Request*>(data));
        // Already on the graphics thread: nativeThumbnail executes its capture directly, never waiting on the UI.
        auto image=nativeThumbnail(request->source);obs_source_release(request->source);
        if(request->owner)QMetaObject::invokeMethod(request->owner,[completed=std::move(request->completed),image=std::move(image)]()mutable{completed(std::move(image));},Qt::QueuedConnection);
    },request,false);
}}
