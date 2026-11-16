#include "obs-native.hpp"
#include <obs-module.h>
#include <util/base.h>
#include <graphics/graphics.h>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <QJsonDocument>
#include <memory>
#include <cstdarg>
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

static int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw jp::Error(QString("Render line %1: %2").arg(__LINE__).arg(#x));}while(false)
using Source=std::unique_ptr<obs_source_t,decltype(&obs_source_release)>;
static void waitFrames(int milliseconds=400){QElapsedTimer timer;timer.start();while(timer.elapsed()<milliseconds){QCoreApplication::processEvents();QThread::msleep(5);}}
static QImage capture(obs_source_t *source){
    struct Request{obs_source_t *source;QImage image;};Request request{source,{}};
    obs_queue_task(OBS_TASK_GRAPHICS,[](void *data){obs_enter_graphics();auto &r=*static_cast<Request*>(data);const auto w=obs_source_get_width(r.source),h=obs_source_get_height(r.source);auto *texture=gs_texrender_create(GS_BGRA,GS_ZS_NONE);auto *stage=gs_stagesurface_create(w,h,GS_BGRA);
        gs_viewport_push();gs_projection_push();gs_matrix_push();
        if(texture&&stage&&gs_texrender_begin(texture,w,h)){vec4 clear{};gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);gs_matrix_identity();obs_source_video_render(r.source);gs_texrender_end(texture);gs_stage_texture(stage,gs_texrender_get_texture(texture));uint8_t *pixels=nullptr;uint32_t stride=0;if(gs_stagesurface_map(stage,&pixels,&stride)){r.image=QImage(pixels,int(w),int(h),int(stride),QImage::Format_ARGB32_Premultiplied).copy();gs_stagesurface_unmap(stage);}else blog(LOG_ERROR,"Stage map failed");}else blog(LOG_ERROR,"Capture target failed: %ux%u texture=%p stage=%p",w,h,texture,stage);
        gs_matrix_pop();gs_projection_pop();gs_viewport_pop();if(stage)gs_stagesurface_destroy(stage);if(texture)gs_texrender_destroy(texture);obs_leave_graphics();
    },&request,true);return request.image;
}
static void load(const QString &root,const QString &name){obs_module_t *module=nullptr;const bool modern=QFileInfo::exists(root+"/core/"+name+"/"+name+".dll");const auto binary=(modern?root+"/core/"+name+"/"+name+".dll":root+"/obs-plugins/64bit/"+name+".dll").toUtf8(),data=(modern?root+"/core/"+name+"/data":root+"/data/obs-plugins/"+name).toUtf8();CHECK(obs_open_module(&module,binary.constData(),data.constData())==MODULE_SUCCESS);CHECK(obs_init_module(module));}
static Source child(obs_source_t *output,const QString &name){struct Query{QString name;obs_source_t *found=nullptr;}query{name};obs_source_enum_active_sources(output,[](obs_source_t*,obs_source_t *source,void *data){auto *scene=obs_scene_from_source(source);if(!scene)return;obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t *item,void *data){auto &query=*static_cast<Query*>(data);auto *source=obs_sceneitem_get_source(item);if(QString::fromUtf8(obs_source_get_name(source))==query.name){query.found=obs_source_get_ref(source);return false;}return true;},data);},&query);return Source(query.found,&obs_source_release);}
static quint64 memoryUsage(){PROCESS_MEMORY_COUNTERS_EX counters{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters));return quint64(counters.PrivateUsage);}
static void test(int stressSeconds){
    QTemporaryDir temp;CHECK(temp.isValid());QImage image(80,40,QImage::Format_ARGB32);image.fill(QColor(255,0,0,255));CHECK(image.save(temp.path()+"/red.png"));
    jp::Store store(temp.path()+"/library");store.open();const auto pid=store.snapshot()["active"].toString();store.resolution(pid,640,360);const auto id=store.importFile(pid,temp.path()+"/red.png","Logos");store.updateMedia(pid,id,{{"draft",QJsonObject{{"x",80},{"y",90},{"width",80},{"height",40},{"transition","none"},{"opacity",100}}}});store.setLive(pid,id,true);
    auto *settings=obs_data_create();const auto state=QJsonDocument(jp::nativeState(store,store.project(pid))).toJson(QJsonDocument::Compact);obs_data_set_string(settings,"state",state.constData());Source program(obs_source_create_private(jp::nativeSourceId,"Test program",settings),&obs_source_release);Source preview(obs_source_create_private(jp::nativeSourceId,"Test preview",settings),&obs_source_release);obs_data_release(settings);CHECK(program);CHECK(preview);obs_source_inc_active(program.get());obs_source_inc_showing(preview.get());
    obs_set_output_source(0,program.get());
    try{
        waitFrames(800);auto frame=capture(program.get());CHECK(!frame.isNull());CHECK(frame.size()==QSize(640,360));CHECK(frame.pixelColor(100,100).alpha()>240);CHECK(frame.pixelColor(100,100).red()>240);CHECK(frame.pixelColor(10,10).alpha()==0);const auto thumb=jp::nativeThumbnail(program.get());CHECK(!thumb.isNull());CHECK(thumb.width()<=320&&thumb.height()<=180);CHECK(jp::nativeObserved(program.get())["items"].toArray().first().toObject()["id"]==id);
        store.updateMedia(pid,id,{{"draft",QJsonObject{{"x",300},{"opacity",50}}}});const auto media=store.project(pid)["media"].toArray().first().toObject();jp::updateNativeSource(preview.get(),jp::nativeState(store,store.project(pid),id,media["draft"].toObject()));waitFrames();frame=capture(program.get());CHECK(frame.pixelColor(100,100).alpha()>240);CHECK(frame.pixelColor(320,100).alpha()==0);auto preparation=capture(preview.get());CHECK(preparation.pixelColor(100,100).alpha()==0);CHECK(preparation.pixelColor(320,100).alpha()>110&&preparation.pixelColor(320,100).alpha()<145);
        store.setLive(pid,id,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames();frame=capture(program.get());CHECK(frame.pixelColor(100,100).alpha()==0);CHECK(frame.pixelColor(320,100).alpha()>110&&frame.pixelColor(320,100).alpha()<145);
        store.updateMedia(pid,id,{{"draft",QJsonObject{{"rotation",90},{"opacity",100},{"anchor","center"},{"x",320},{"y",180}}}});store.setLive(pid,id,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames();frame=capture(program.get());CHECK(frame.pixelColor(320,180).alpha()>240);CHECK(frame.pixelColor(320,150).alpha()>240);CHECK(frame.pixelColor(350,180).alpha()==0);
        const auto title=store.createTitle(pid,"Título",{{"name","Maria Silva"},{"role","Repórter"},{"city","Curitiba"}});store.setLive(pid,title,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(900);frame=capture(program.get());QDir().mkpath("artifacts");CHECK(frame.pixelColor(40,300).alpha()>200);
        QDir().mkpath("artifacts");CHECK(frame.save("artifacts/native-compositor.png"));CHECK(preparation.save("artifacts/native-private-preview.png"));
        store.clearLive(pid);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames();frame=capture(program.get());CHECK(frame.pixelColor(320,180).alpha()==0);CHECK(frame.pixelColor(40,300).alpha()==0);
        // Real local VP8/WebM and H.264/MP4, decoded by the actual OBS FFmpeg module.
        QStringList videos;
        for(const auto &extension:QStringList{"webm","mp4"}){const auto file=QDir("tests/fixtures").absoluteFilePath("sample."+extension);CHECK(QFile::exists(file));const auto clip=store.importFile(pid,file,"Vinhetas");store.updateMedia(pid,clip,{{"name","clip-"+extension},{"draft",QJsonObject{{"x",extension=="webm"?50:200},{"y",50},{"width",80},{"height",40},{"loop",false},{"transition","none"}}}});store.setLive(pid,clip,true);videos<<clip;}
        jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(700);frame=capture(program.get());auto playing=child(program.get(),"clip-webm");CHECK(playing);CHECK(frame.pixelColor(60,60).alpha()>240);CHECK(frame.pixelColor(210,60).alpha()>240);const auto time=obs_source_media_get_time(playing.get());CHECK(time>0);
        store.setLive(pid,id,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(250);auto same=child(program.get(),"clip-webm");CHECK(same.get()==playing.get());CHECK(obs_source_media_get_time(playing.get())>=time);
        waitFrames(1600);frame=capture(program.get());CHECK(frame.pixelColor(60,60).alpha()>240);CHECK(frame.pixelColor(210,60).alpha()>240);CHECK(obs_source_media_get_state(playing.get())==OBS_MEDIA_STATE_ENDED);CHECK(jp::nativeObserved(program.get())["ended"].toArray().size()==2);
        // A new TAKE reuses the decoder, restarts at zero and applies the real FFmpeg loop setting.
        const auto looping=videos.first();store.saveCategoryLayouts(pid,{{"Vinhetas",QJsonObject{{"x",50},{"y",50},{"width",80},{"height",40}}}},{640,360});store.updateMedia(pid,looping,{{"draft",QJsonObject{{"loop",true}}}});store.take(pid,looping);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(500);auto restarted=child(program.get(),"clip-webm");CHECK(restarted.get()==playing.get());CHECK(obs_source_media_get_time(restarted.get())<1000);auto *loopSettings=obs_source_get_settings(restarted.get());CHECK(obs_data_get_bool(loopSettings,"looping"));obs_data_release(loopSettings);waitFrames(2600);CHECK(obs_source_media_get_state(restarted.get())==OBS_MEDIA_STATE_PLAYING);CHECK(jp::nativeObserved(program.get())["ended"].toArray().isEmpty());
        const auto gif=store.importFile(pid,QDir("tests/fixtures").absoluteFilePath("sample.gif"),"Vinhetas");store.updateMedia(pid,gif,{{"draft",QJsonObject{{"x",400},{"y",50},{"width",80},{"height",40},{"transition","none"}}}});store.setLive(pid,gif,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(300);QSet<QRgb> colors;
        for(int i=0;i<8;++i){frame=capture(program.get());CHECK(frame.pixelColor(420,60).alpha()>240);colors.insert(frame.pixel(420,60));waitFrames(110);}CHECK(colors.size()>1);
        store.updateMedia(pid,id,{{"draft",QJsonObject{{"x",100},{"y",200},{"anchor","top-left"},{"rotation",0},{"transition","fade"},{"duration",300}}}});store.setLive(pid,id,true);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(450);CHECK(capture(program.get()).pixelColor(120,210).alpha()>240);store.setLive(pid,id,false);jp::updateNativeSource(program.get(),jp::nativeState(store,store.project(pid)));waitFrames(100);const auto during=capture(program.get()).pixelColor(120,210).alpha();CHECK(during>0&&during<255);waitFrames(400);CHECK(capture(program.get()).pixelColor(120,210).alpha()==0);
        if(stressSeconds>0){
            for(const auto &clip:videos){store.updateMedia(pid,clip,{{"draft",QJsonObject{{"loop",true}}}});store.setLive(pid,clip,false);store.setLive(pid,clip,true);}store.setLive(pid,id,true);auto state=jp::nativeState(store,store.project(pid));jp::updateNativeSource(program.get(),state);waitFrames(1000);
            const auto initial=memoryUsage();auto peak=initial;QElapsedTimer timer;timer.start();int updates=0;
            while(timer.elapsed()<stressSeconds*1000){auto items=state["items"].toArray();for(int i=0;i<items.size();++i){auto item=items[i].toObject();auto layout=item["layout"].toObject();layout["rotation"]=(updates%40)-20;layout["opacity"]=50+(updates%51);item["layout"]=layout;items[i]=item;}state["items"]=items;jp::updateNativeSource(program.get(),state);if(updates%20==0){const auto frame=capture(program.get());CHECK(!frame.isNull());peak=qMax(peak,memoryUsage());}++updates;waitFrames(20);}
            waitFrames(500);const auto finalMemory=memoryUsage();CHECK(finalMemory<initial+64ULL*1024*1024);QFile result("artifacts/renderer-stress.json");CHECK(result.open(QIODevice::WriteOnly));result.write(QJsonDocument(QJsonObject{{"passed",true},{"durationSeconds",stressSeconds},{"updates",updates},{"privateBytesInitial",double(initial)},{"privateBytesFinal",double(finalMemory)},{"privateBytesPeak",double(peak)},{"scope","isolated libobs D3D11; two videos, GIF and PNG; not a four-hour production soak"}}).toJson());
        }
        auto high=jp::nativeState(store,store.project(pid),id,store.project(pid)["media"].toArray().first().toObject()["draft"].toObject());high["width"]=3840;high["height"]=2160;auto highItems=high["items"].toArray();auto highItem=highItems[0].toObject();auto highLayout=highItem["layout"].toObject();highLayout["x"]=3400;highLayout["y"]=1800;highLayout["cropLeft"]=20;highLayout["rotation"]=0;highLayout["opacity"]=100;highItem["layout"]=highLayout;highItems[0]=highItem;high["items"]=highItems;jp::updateNativeSource(program.get(),high);waitFrames(400);const auto fourK=capture(program.get());CHECK(fourK.size()==QSize(3840,2160));CHECK(fourK.pixelColor(3420,1810).alpha()>240);CHECK(fourK.pixelColor(3470,1810).alpha()==0);CHECK(fourK.save("artifacts/native-compositor-4k.png"));
    }catch(...){obs_set_output_source(0,nullptr);obs_source_dec_active(program.get());obs_source_dec_showing(preview.get());throw;}
    obs_set_output_source(0,nullptr);obs_source_dec_active(program.get());obs_source_dec_showing(preview.get());
}
int main(int argc,char **argv){QApplication app(argc,argv);bool started=false;
    base_set_log_handler([](int level,const char *format,va_list args,void*){if(level<=LOG_WARNING){char message[4096];vsnprintf(message,sizeof(message),format,args);QTextStream(stderr)<<message<<"\n";}},nullptr);
    try{CHECK(obs_startup("pt-BR",nullptr,nullptr));started=true;const auto arguments=app.arguments();const int runtime=arguments.indexOf("--obs-root");const auto root=QDir(runtime>=0?arguments.value(runtime+1):".deps/obs-test").absolutePath();CHECK(AddDllDirectory(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(root+"/bin/64bit").utf16()))!=nullptr);const auto data=(root+"/data/libobs/").toUtf8();obs_add_data_path(data.constData());
        obs_video_info video{};const auto module=(root+"/bin/64bit/libobs-d3d11.dll").toUtf8();video.graphics_module=module.constData();video.fps_num=30;video.fps_den=1;video.base_width=640;video.base_height=360;video.output_width=640;video.output_height=360;video.output_format=VIDEO_FORMAT_RGBA;video.colorspace=VIDEO_CS_709;video.range=VIDEO_RANGE_FULL;CHECK(obs_reset_video(&video)==OBS_VIDEO_SUCCESS);
        obs_audio_info audio{};audio.samples_per_sec=48000;audio.speakers=SPEAKERS_STEREO;CHECK(obs_reset_audio(&audio));load(root,"image-source");load(root,"obs-filters");load(root,"obs-ffmpeg");jp::registerNativeSource();const auto args=app.arguments();const int stress=args.indexOf("--stress-seconds");test(stress>=0?qBound(1,args.value(stress+1).toInt(),14400):0);obs_shutdown();started=false;QTextStream(stdout)<<"PASS native OBS compositor (D3D11), alpha, transforms, private preview, TAKE/OUT: "<<checks<<" checks\n";return 0;
    }catch(const std::exception &e){QTextStream(stderr)<<"FAIL after "<<checks<<" checks: "<<e.what()<<"\n";if(started)obs_shutdown();return 1;}
}
