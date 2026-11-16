#include "controller.hpp"
#include <QDateTime>
#include <QTimer>
#include <QDebug>

namespace jp {
Controller::Controller(Store &s,Downloader &d,QObject *parent):QObject(parent),store(s),downloader(d){
    lastProject_=projectId();
    connect(&store,&Store::changed,this,[this]{if(refreshPending_)return;refreshPending_=true;QTimer::singleShot(0,this,[this]{refreshPending_=false;refresh();});});
    connect(&downloader,&Downloader::completed,this,&Controller::notify);
    connect(&store,&Store::importFinished,this,&Controller::notify);
}
QString Controller::projectId()const{return store.snapshot()["active"].toString();}
QJsonObject Controller::project()const{if(projectId().isEmpty())return {};return store.project(projectId());}
QJsonObject Controller::selected()const{for(const auto &v:project()["media"].toArray())if(v.toObject()["id"].toString()==selectedId)return v.toObject();return {};}
void Controller::refresh(){
    if(lastProject_!=projectId()||selected().isEmpty()){selectedId.clear();draft={};dirty=false;lastProject_=projectId();confirmed=confirmedProjects.value(lastProject_);}
    else if(!dirty)draft=selected()["draft"].toObject();
    emit changed();emit draftChanged();
}
void Controller::select(const QString &id){
    if(selectedId==id)return;selectedId=id;
    if(selected().isEmpty()){selectedId.clear();draft={};}else draft=selected()["draft"].toObject();dirty=false;emit changed();emit draftChanged();emit selectionChanged();
}
void Controller::edit(const QJsonObject &patch){draft=Store::validatedLayout(draft,patch);dirty=true;emit draftChanged();}
void Controller::save(){if(selected().isEmpty())throw Error("Selecione um elemento.");store.updateMedia(projectId(),selectedId,{{"draft",draft}});dirty=false;notify("Preparação salva. TAKE publica as alterações.");}
void Controller::take(){if(!online||!linked)throw Error("Integração OBS indisponível.");if(selected().isEmpty())throw Error("Selecione uma peça.");if(selected()["mime"]!="application/x-jp-title")store.mediaPath(selected());if(preflightTake)preflightTake();save();store.take(projectId(),selectedId);pendingId_=selectedId;pendingProject_=projectId();pendingName_=selected()["name"].toString();pendingGeneration_=selected()["generation"].toInt();pendingOn_=true;notify("TAKE enviado · aguardando confirmação no programa do OBS.");}
void Controller::out(){if(!online||!linked)throw Error("Integração OBS indisponível.");if(selected().isEmpty())throw Error("Selecione o conteúdo que deseja retirar.");store.setLive(projectId(),selectedId,false);pendingId_=selectedId;pendingProject_=projectId();pendingName_=selected()["name"].toString();pendingOn_=false;notify("OUT enviado · aguardando confirmação do OBS.");}
void Controller::confirm(const QSet<QString> &ids,const QMap<QString,int> &generations){
    confirmedProjects[projectId()]=ids;
    if(!pendingId_.isEmpty()&&pendingProject_==projectId()&&(pendingOn_?(ids.contains(pendingId_)&&(generations.isEmpty()||generations.value(pendingId_,-1)==pendingGeneration_)):!ids.contains(pendingId_))){notify((pendingOn_?"No ar confirmado pelo OBS · ":"Retirada confirmada pelo OBS · ")+pendingName_);pendingId_.clear();}
    if(confirmed==ids)return;confirmed=ids;emit changed();
}
void Controller::confirmProjects(const QMap<QString,QSet<QString>> &ids,const QMap<QString,QMap<QString,int>> &generations){auto normalized=ids;normalized[projectId()];const bool allChanged=confirmedProjects!=normalized;confirmedProjects=normalized;const auto before=confirmed;confirm(normalized.value(projectId()),generations.value(projectId()));if(allChanged&&before==confirmed)emit changed();}
void Controller::confirmProvider(const QString &provider,const QMap<QString,QSet<QString>> &ids,const QMap<QString,QMap<QString,int>> &generations){providers_[provider]=ids;providerGenerations_[provider]=generations;QMap<QString,QSet<QString>> merged;QMap<QString,QMap<QString,int>> versions;for(auto p=providers_.begin();p!=providers_.end();++p)for(auto it=p.value().begin();it!=p.value().end();++it){merged[it.key()].unite(it.value());const auto observed=providerGenerations_.value(p.key()).value(it.key());for(auto v=observed.begin();v!=observed.end();++v)versions[it.key()][v.key()]=qMax(versions[it.key()].value(v.key(),-1),v.value());}confirmProjects(merged,versions);}
QStringList Controller::otherProjectsOnAir()const{QStringList result;for(const auto &v:store.snapshot()["projects"].toArray()){const auto p=v.toObject();if(p["id"].toString()!=projectId()&&!confirmedProjects.value(p["id"].toString()).isEmpty())result<<p["name"].toString();}return result;}
void Controller::switchProject(const QString &id){
    store.project(id);const auto previous=lastProject_;const bool wasLive=!confirmedProjects.value(previous).isEmpty();QString oldName;try{oldName=store.project(previous)["name"].toString();}catch(const Error &){}
    if(projectId()!=id)store.selectProject(id);selectedId.clear();draft={};dirty=false;lastProject_=id;confirmed=confirmedProjects.value(id);emit changed();emit draftChanged();emit selectionChanged();
    if(wasLive&&previous!=id)notify("O projeto '"+oldName+"' continua no programa. A troca de projeto não altera a saída do OBS.");
}
void Controller::link(bool enabled){linked=enabled;if(!enabled)setObsState(false,false,false,"Controle desconectado · saída OBS preservada");else if(reconnect)reconnect();emit changed();}
void Controller::sync(const QString &source,const QStringList &targets,const QStringList &fields){
    const auto changes=store.synchronize(projectId(),source,targets,fields);undo_.append({projectId(),changes});if(undo_.size()>50)undo_.removeFirst();redo_.clear();
    if(!selected().isEmpty())draft=selected()["draft"].toObject();dirty=false;notify(QString("Sincronização aplicada a %1 elemento(s), somente na preparação.").arg(changes.size()));
}
void Controller::syncCategories(const QString &source,const QStringList &targets,const QStringList &fields){const auto changes=store.synchronizeCategories(projectId(),source,targets,fields);undo_.append({projectId(),changes,true});if(undo_.size()>50)undo_.removeFirst();redo_.clear();notify("Posições sincronizadas entre categorias, somente na preparação.");}
void Controller::undo(){int index=-1;for(int i=int(undo_.size())-1;i>=0;--i)if(undo_[i].project==projectId()){index=i;break;}if(index<0)throw Error("Nenhuma sincronização para desfazer neste projeto.");const auto op=undo_[index];if(op.categories)store.restoreCategoryLayouts(op.project,op.changes,true);else store.restoreLayouts(op.project,op.changes,true);undo_.removeAt(index);redo_.append(op);dirty=false;notify("Sincronização desfeita.");}
void Controller::redo(){int index=-1;for(int i=int(redo_.size())-1;i>=0;--i)if(redo_[i].project==projectId()){index=i;break;}if(index<0)throw Error("Nenhuma sincronização para refazer neste projeto.");const auto op=redo_[index];if(op.categories)store.restoreCategoryLayouts(op.project,op.changes,false);else store.restoreLayouts(op.project,op.changes,false);redo_.removeAt(index);undo_.append(op);dirty=false;notify("Sincronização refeita.");}
void Controller::setObsState(bool integrated,bool stream,bool record,const QString &status){integrated=integrated&&linked;if(online==integrated&&streaming==stream&&recording==record&&obsStatus==status)return;online=integrated;streaming=stream;recording=record;obsStatus=status;if(!online)confirmed.clear();emit changed();}
void Controller::notify(const QString &message,bool error){history.prepend(QDateTime::currentDateTime().toString("HH:mm:ss")+" · "+message);while(history.size()>200)history.removeLast();qInfo().noquote()<<"[JP Fachina Graphics]"<<message;emit notice(message,error);}
}
