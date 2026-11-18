#include "position-editor.hpp"
#include "title-image.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QImageReader>
#include <QShortcut>
#include <QTimer>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QInputDialog>
#include <QScreen>
#include <QGuiApplication>
#include <QLineEdit>
#include "wrap-layout.hpp"

namespace jp {
PositionSession::PositionSession(Controller &controller,QObject *parent):QObject(parent),c_(controller){}
void PositionSession::open(const QString &category){
    project_=c_.projectId();if(project_.isEmpty())throw Error("Crie um projeto em Cenas.");
    const auto p=c_.store.project(project_);frame_=c_.videoSize?c_.videoSize():QSize(p["width"].toInt(1920),p["height"].toInt(1080));
    if(!frame_.isValid())frame_={p["width"].toInt(1920),p["height"].toInt(1080)};
    originals_.clear();drafts_.clear();for(const auto &cat:panelCategories())drafts_[cat]=c_.store.categoryLayout(project_,cat,frame_);
    for(const auto &v:p["media"].toArray()){const auto m=v.toObject();if(m["mime"]!="application/x-jp-scoreboard")continue;scoreboardId_=m["id"].toString();drafts_["Scoreboard"]=positionForFrame(m["draft"].toObject(),{p["width"].toInt(),p["height"].toInt()},frame_);break;}
    originals_=drafts_;category_=drafts_.contains(category)?category:"Logos";loadImage();emit changed();
}
QJsonObject PositionSession::media()const{
    const auto p=c_.store.project(project_);const auto selected=c_.selected();
    if(category_=="Scoreboard"){for(const auto &v:p["media"].toArray())if(v.toObject()["id"].toString()==scoreboardId_)return v.toObject();return {};}
    if(c_.projectId()==project_&&!selected.isEmpty()&&panelCategory(selected["category"].toString(),selected["mime"].toString())==category_)return selected;
    for(const auto &v:p["media"].toArray()){const auto m=v.toObject();if(panelCategory(m["category"].toString(),m["mime"].toString())==category_)return m;}return {};
}
void PositionSession::loadImage(){
    image_={};const auto m=media();if(m.isEmpty())return;
    if(m["mime"]=="application/x-jp-scoreboard"){image_=QImage(c_.store.scoreboardImagePath(c_.store.project(project_),m)).scaled(960,540,Qt::KeepAspectRatio,Qt::SmoothTransformation);return;}
    if(m["mime"]=="application/x-jp-title"){image_=titleImage(m["draft"].toObject()).scaled(960,540,Qt::KeepAspectRatio,Qt::SmoothTransformation);return;}
    if(m["mime"].toString().startsWith("video/")&&!m.contains("thumbnail"))return;
    try{QImageReader reader(c_.store.mediaPath(m,m.contains("thumbnail")));reader.setAutoTransform(true);const auto size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(960,540,Qt::KeepAspectRatio));image_=reader.read();}catch(const Error &){}
}
void PositionSession::category(const QString &category){if(!drafts_.contains(category))throw Error("Categoria inválida.");category_=category;loadImage();emit changed();}
void PositionSession::edit(const QJsonObject &patch){for(auto it=patch.begin();it!=patch.end();++it)if(!positionFields().contains(it.key()))throw Error("Propriedade de posição inválida.");drafts_[category_]=Store::validatedLayout(layout(),patch);emit changed();}
void PositionSession::move(double x,double y){const auto d=layout();edit({{"x",qBound(-10000.,d["x"].toDouble()+x,10000.)},{"y",qBound(-10000.,d["y"].toDouble()+y,10000.)}});}
void PositionSession::reset(){if(category_=="Scoreboard"){const auto p=c_.store.project(project_);const auto m=media();const double w=qMin(frame_.width()*.9,frame_.height()*.35*1920./360.),h=w*360./1920.;drafts_[category_]=Store::validatedLayout(m["draft"].toObject(),{{"x",(frame_.width()-w)/2},{"y",frame_.height()*.05},{"width",w},{"height",h},{"scale",1},{"rotation",0},{"anchor","top-left"}});}else drafts_[category_]=c_.store.defaultCategoryLayout(project_,category_,frame_);emit changed();}
void PositionSession::save(bool applyLive){
    if(applyLive&&!c_.online)throw Error("Conecte ao OBS antes de aplicar a posição no ar.");
    if(category_=="Scoreboard"){if(scoreboardId_.isEmpty())throw Error("Selecione um scoreboard na ABA 3.");const auto p=c_.store.project(project_);const auto converted=positionForFrame(layout(),frame_,{p["width"].toInt(),p["height"].toInt()});c_.store.updateScoreboardPosition(project_,scoreboardId_,converted,applyLive);originals_[category_]=layout();emit changed();c_.notify(applyLive?"Scoreboard atualizado no OBS.":"Posição do scoreboard salva para o próximo Take.");return;}
    QJsonObject changes;if(layout()!=originals_.value(category_)||applyLive)changes[category_]=layout();
    if(!changes.isEmpty())c_.store.saveCategoryLayouts(project_,changes,frame_,applyLive);originals_[category_]=layout();emit changed();
    c_.notify(applyLive?"Posições salvas e aplicação solicitada ao OBS.":"Posições por categoria salvas para os próximos Takes.");
}
void PositionSession::cancel(){drafts_=originals_;emit changed();}

PositionDialog::PositionDialog(Controller &controller,QWidget *parent):QDialog(parent),c_(controller),session_(controller,this){
    setObjectName("jpPositionDialog");setWindowTitle("Editar posição por categoria");setWindowModality(Qt::WindowModal);resize(1120,790);
    setStyleSheet("QDialog{background:#181818;color:white;font-family:'Segoe UI';} QLabel,QCheckBox{color:white;} QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #009cff,stop:1 #29218f);color:white;border:2px solid #4865b6;border-radius:12px;padding:8px 14px;font-weight:700;} QPushButton:focus{border-color:white;} QDoubleSpinBox,QComboBox{background:#262626;color:white;border:1px solid #686868;padding:6px;}");
    auto *root=new QVBoxLayout(this);auto *top=new QFormLayout;category_=new QComboBox(this);category_->setObjectName("jpPositionCategory");category_->addItems(panelCategories());category_->addItem("Scoreboard");top->addRow("Categoria",category_);root->addLayout(top);
    auto *body=new QHBoxLayout;canvas_=new PositionCanvas(session_,this);canvas_->setObjectName("jpPositionCanvas");canvas_->setFocusPolicy(Qt::StrongFocus);body->addWidget(canvas_,1);auto *controlsPanel=new QWidget(this);controlsPanel->setMinimumWidth(280);controlsPanel->setMaximumWidth(340);auto *controls=new QVBoxLayout(controlsPanel);controls->setContentsMargins(0,0,0,0);auto *details=new QWidget(controlsPanel);auto *detailLayout=new QVBoxLayout(details);detailLayout->setContentsMargins(0,0,0,0);auto *detailScroll=new QScrollArea(controlsPanel);detailScroll->setWidgetResizable(true);detailScroll->setMinimumSize(0,0);detailScroll->setFrameShape(QFrame::NoFrame);detailScroll->setWidget(details);controls->addWidget(detailScroll,1);
    auto *grid=new QGridLayout;const QList<QPair<QString,QString>> fields{{"x","Horizontal · X"},{"y","Vertical · Y"},{"width","Largura da área"},{"height","Altura da área"},{"scale","Escala"},{"rotation","Rotação °"}};int index=0;
    for(const auto &pair:fields){auto *box=new QDoubleSpinBox(this);box->setObjectName("jpPosition_"+pair.first);box->setDecimals(2);box->setRange(-10000,10000);if(pair.first=="width"||pair.first=="height")box->setRange(1,16384);if(pair.first=="rotation")box->setRange(-360,360);if(pair.first=="scale"){box->setRange(.01,20);box->setSingleStep(.05);}fields_[pair.first]=box;grid->addWidget(new QLabel(pair.second,this),index/2*2,index%2);grid->addWidget(box,index/2*2+1,index%2);++index;
        connect(box,&QDoubleSpinBox::valueChanged,this,[this,key=pair.first](double value){if(rendering_)return;run([this,key,value]{QJsonObject patch{{key,value}};const auto d=session_.layout();if(aspect_->isChecked()&&(key=="width"||key=="height")){const auto other=key=="width"?"height":"width";patch[other]=qBound(1.,d[other].toDouble(1)*value/d[key].toDouble(1),16384.);}session_.edit(patch);});});
    }detailLayout->addLayout(grid);aspect_=new QCheckBox("Preservar proporção da área",this);aspect_->setChecked(true);detailLayout->addWidget(aspect_);connect(aspect_,&QCheckBox::toggled,canvas_,&PositionCanvas::setPreserveAspect);
    anchor_=new QComboBox(this);alignment_=new QComboBox(this);for(const auto &p:QList<QPair<QString,QString>>{{"top-left","Superior esquerdo"},{"top-center","Superior centro"},{"top-right","Superior direito"},{"center-left","Centro esquerdo"},{"center","Centro"},{"center-right","Centro direito"},{"bottom-left","Inferior esquerdo"},{"bottom-center","Inferior centro"},{"bottom-right","Inferior direito"}}){anchor_->addItem(p.second,p.first);alignment_->addItem(p.second,p.first);}auto *align=new QFormLayout;align->addRow("Âncora",anchor_);align->addRow("Alinhamento na área",alignment_);detailLayout->addLayout(align);detailLayout->addStretch();
    for(const auto &pair:QList<QPair<QComboBox*,QString>>{{anchor_,"anchor"},{alignment_,"alignment"}})connect(pair.first,&QComboBox::currentIndexChanged,this,[this,box=pair.first,key=pair.second]{if(!rendering_)run([this,box,key]{session_.edit({{key,box->currentData().toString()}});});});
    step_=new QComboBox(this);step_->setObjectName("jpPositionStep");step_->addItem("Fino · 1 pixel",1);step_->addItem("Normal · 10 pixels",10);step_->addItem("Rápido · 50 pixels",50);step_->setCurrentIndex(1);controls->addWidget(step_);
    auto *arrows=new QGridLayout;const QList<QPair<QString,QPoint>> directions{{"▲",{0,-1}},{"◀",{-1,0}},{"●",{0,0}},{"▶",{1,0}},{"▼",{0,1}}};const QList<QPoint> cells{{1,0},{0,1},{1,1},{2,1},{1,2}};
    for(int i=0;i<directions.size();++i){auto *b=new QPushButton(directions[i].first,this);b->setObjectName(QStringList{"jpMoveUp","jpMoveLeft","jpMoveCenter","jpMoveRight","jpMoveDown"}[i]);b->setAccessibleName(QStringList{"Mover para cima","Mover para esquerda","Centralizar categoria","Mover para direita","Mover para baixo"}[i]);b->setFixedSize(84,72);b->setStyleSheet("font-size:28px;padding:0;");b->setAutoRepeat(i!=2);b->setAutoRepeatDelay(300);b->setAutoRepeatInterval(50);arrows->addWidget(b,cells[i].y(),cells[i].x());connect(b,&QPushButton::clicked,this,[this,delta=directions[i].second]{if(delta.isNull())run([this]{session_.edit({{"x",session_.frame().width()/2.},{"y",session_.frame().height()/2.},{"anchor","center"}});});else nudge(delta.x(),delta.y());});}
    controls->addLayout(arrows);auto *edges=new QGridLayout;const QList<QPair<QString,QString>> positions{{"Esquerda","left"},{"Direita","right"},{"Topo","top"},{"Base","bottom"},{"Centro H","horizontal"},{"Centro V","vertical"}};for(int i=0;i<positions.size();++i){auto *b=new QPushButton(positions[i].first,this);b->setObjectName("jpAlign_"+positions[i].second);edges->addWidget(b,i/2,i%2);connect(b,&QPushButton::clicked,this,[this,side=positions[i].second]{run([this,side]{const auto d=session_.layout();const double w=d["width"].toDouble()*d["scale"].toDouble(1),h=d["height"].toDouble()*d["scale"].toDouble(1);QJsonObject patch{{"anchor","top-left"}};if(side=="left")patch["x"]=0;if(side=="right")patch["x"]=session_.frame().width()-w;if(side=="top")patch["y"]=0;if(side=="bottom")patch["y"]=session_.frame().height()-h;if(side=="horizontal")patch["x"]=(session_.frame().width()-w)/2;if(side=="vertical")patch["y"]=(session_.frame().height()-h)/2;session_.edit(patch);});});}controls->addLayout(edges);coordinates_=new QLabel(this);coordinates_->setObjectName("jpPositionCoordinates");coordinates_->setWordWrap(true);controls->addWidget(coordinates_);body->addWidget(controlsPanel);auto *bodyHost=new QWidget(this);bodyHost->setLayout(body);auto *bodyScroll=new QScrollArea(this);bodyScroll->setWidgetResizable(true);bodyScroll->setMinimumSize(0,0);bodyScroll->setFrameShape(QFrame::NoFrame);bodyScroll->viewport()->setAutoFillBackground(false);bodyScroll->setWidget(bodyHost);root->addWidget(bodyScroll,1);
    status_=new QLabel("Edição local. Salvar prepara os próximos Takes; Aplicar também ajusta a categoria no ar.",this);status_->setWordWrap(true);root->addWidget(status_);auto *buttons=new WrapLayout;
    auto add=[&](const QString &label,const QString &name,const std::function<void()> &fn){auto *b=new QPushButton(label,this);b->setObjectName(name);buttons->addWidget(b);connect(b,&QPushButton::clicked,this,fn);};
    add("Restaurar padrão","jpPositionReset",[this]{run([this]{session_.reset();});});
    add("Recorte…","jpPositionCrop",[this]{run([this]{QDialog dialog(this);dialog.setWindowTitle("Recorte da categoria");auto *layout=new QVBoxLayout(&dialog);auto *form=new QFormLayout;QMap<QString,QDoubleSpinBox*> fields;for(const auto &pair:QList<QPair<QString,QString>>{{"cropLeft","Esquerdo"},{"cropRight","Direito"},{"cropTop","Superior"},{"cropBottom","Inferior"}}){auto *box=new QDoubleSpinBox(&dialog);box->setRange(0,16384);box->setValue(session_.layout().value(pair.first).toDouble());fields[pair.first]=box;form->addRow(pair.second,box);}layout->addLayout(form);auto *actions=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(actions);connect(actions,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(actions,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()==QDialog::Accepted){QJsonObject patch;for(auto it=fields.begin();it!=fields.end();++it)patch[it.key()]=it.value()->value();session_.edit(patch);}});});
    add("Modelos…","jpPositionModels",[this]{run([this]{bool ok=false;const auto action=QInputDialog::getItem(this,"Modelos de posição","Ação",{"Salvar categoria como modelo","Aplicar modelo à categoria"},0,false,&ok);if(!ok)return;const auto pid=session_.projectId();const auto project=c_.store.project(pid);const QSize frame(project["width"].toInt(1920),project["height"].toInt(1080));if(action=="Salvar categoria como modelo"){const auto name=QInputDialog::getText(this,"Salvar modelo","Nome",QLineEdit::Normal,session_.category(),&ok);if(ok)c_.store.saveTemplate(pid,name,positionForFrame(session_.layout(),session_.frame(),frame));}else{const auto names=project["templates"].toObject().keys();if(names.isEmpty())throw Error("Nenhum modelo salvo neste projeto.");const auto name=QInputDialog::getItem(this,"Aplicar modelo","Modelo",names,0,false,&ok);if(ok){const auto model=project["templates"].toObject()[name].toObject();QJsonObject position;for(const auto &key:positionFields())if(model.contains(key))position[key]=model[key];session_.edit(positionForFrame(position,frame,session_.frame()));}}});});
    add("Cancelar","jpPositionCancel",[this]{reject();});add("Salvar","jpPositionSave",[this]{run([this]{session_.save();render();});});add("Salvar e aplicar no ar","jpPositionApply",[this]{run([this]{session_.save(true);render();});});root->addLayout(buttons);
    refresh_=new QTimer(this);refresh_->setSingleShot(true);refresh_->setInterval(16);connect(refresh_,&QTimer::timeout,this,&PositionDialog::render);connect(&session_,&PositionSession::changed,this,[this]{if(!refresh_->isActive())refresh_->start();});
    connect(category_,&QComboBox::currentTextChanged,this,[this](const QString &cat){if(!rendering_)run([this,cat]{session_.category(cat);render();});});
    for(const auto &key:QList<QPair<int,QPoint>>{{Qt::Key_Up,{0,-1}},{Qt::Key_Left,{-1,0}},{Qt::Key_Right,{1,0}},{Qt::Key_Down,{0,1}}}){auto *shortcut=new QShortcut(QKeySequence(key.first),this);shortcut->setContext(Qt::WindowShortcut);connect(shortcut,&QShortcut::activated,this,[this,delta=key.second]{nudge(delta.x(),delta.y());});}
    connect(&c_,&Controller::changed,this,[this]{if(isVisible()&&c_.projectId()!=session_.projectId()){reject();c_.notify("Editor fechado: o projeto ativo foi alterado.");}});
}
void PositionDialog::openCategory(const QString &category){session_.open(category);render();auto *display=parentWidget()?parentWidget()->screen():QGuiApplication::primaryScreen();if(display){const auto available=display->availableGeometry();resize(qMin(1120,available.width()-24),qMin(790,available.height()-48));}show();raise();activateWindow();canvas_->setFocus();}
void PositionDialog::reject(){session_.cancel();refresh_->stop();QDialog::reject();}
void PositionDialog::run(const std::function<void()> &action){try{action();}catch(const std::exception &e){status_->setText(QString::fromUtf8(e.what()));}}
void PositionDialog::nudge(int dx,int dy){run([this,dx,dy]{const auto step=step_->currentData().toInt();session_.move(dx*step,dy*step);});}
void PositionDialog::render(){rendering_=true;category_->setCurrentText(session_.category());const auto d=session_.layout();for(auto it=fields_.begin();it!=fields_.end();++it){const QSignalBlocker block(it.value());if(!it.value()->hasFocus())it.value()->setValue(d.value(it.key()).toDouble(it.key()=="scale"?1:0));}anchor_->setCurrentIndex(qMax(0,anchor_->findData(d.value("anchor").toString("top-left"))));alignment_->setCurrentIndex(qMax(0,alignment_->findData(d.value("alignment").toString("center"))));coordinates_->setText(QString("X: %1   Y: %2   ·   %3 × %4%5").arg(d["x"].toDouble(),0,'f',1).arg(d["y"].toDouble(),0,'f',1).arg(session_.frame().width()).arg(session_.frame().height()).arg(session_.dirty()?" · não salvo":" · salvo"));setWindowTitle("Editar posição · "+c_.store.project(session_.projectId())["name"].toString());canvas_->update();rendering_=false;}
}
