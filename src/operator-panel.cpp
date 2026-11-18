#include "operator-panel.hpp"
#include "media-card.hpp"
#include "title-image.hpp"
#include "wrap-layout.hpp"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QMessageBox>
#include <QMenu>
#include <QTimer>
#include <QDialog>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QImageReader>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QAccessible>
#include <QFileInfo>
#include <QDateTime>
#include <QStyle>

static void initUiResources(){Q_INIT_RESOURCE(resources);}
namespace jp {
static QPushButton *button(const QString &label,QLayout *layout,QWidget *parent,const std::function<void()> &action,const QString &name={}){
    auto *b=new QPushButton(label,parent);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);layout->addWidget(b);QObject::connect(b,&QPushButton::clicked,parent,action);return b;
}
OperatorPanel::OperatorPanel(Controller &c,QWidget *parent):QWidget(parent),c_(c),prefPath_(c.store.root()+"/panel.json"){
    initUiResources();QAccessible::setActive(true);setObjectName("jpPanel");setMinimumSize(340,260);setAttribute(Qt::WA_StyledBackground,true);
    setStyleSheet(R"(
      QWidget#jpPanel {background:qradialgradient(cx:0.5,cy:0.48,fx:0.5,fy:0.48,radius:0.65,stop:0 #333333,stop:0.62 #1b1b1b,stop:1 #030303);color:white;font-family:'Segoe UI';font-size:13px;}
      #jpPanel QLabel {color:white;background:transparent;}
      #jpPanel QLabel#jpLogo {background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #08051a,stop:1 #3731c3);border-top-right-radius:28px;border-bottom-right-radius:28px;}
      #jpPanel QPushButton {background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #009cff,stop:1 #29218f);color:white;border:3px solid transparent;border-radius:26px;padding:8px 12px;font-weight:700;}
      #jpPanel QPushButton:hover {border-color:#83cfff;}
      #jpPanel QPushButton:focus {border-color:white;}
      #jpPanel QPushButton:disabled {color:#aaa;background:#393939;}
      #jpPanel QPushButton#jpTake,#jpPanel QPushButton#jpOut,#jpPanel QPushButton#jpLink {background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #ff3538,stop:1 #f62532);}
      #jpPanel QPushButton[active="true"] {border-color:white;}
      #jpPanel QScrollArea,#jpPanel QStackedWidget {background:transparent;border:0;}
      #jpPanel QCheckBox {color:white;spacing:7px;}
      QDialog {background:#191919;color:#fff;font-family:'Segoe UI';font-size:13px;}
      QDialog QLabel,QDialog QCheckBox {color:#eee;}
      QDialog QLineEdit,QDialog QComboBox,QDialog QDoubleSpinBox,QDialog QSpinBox,QDialog QListWidget {background:#272727;color:#fff;border:1px solid #626262;padding:5px;}
      QDialog QPushButton {background:#283aa4;color:white;border:1px solid #647ac8;border-radius:8px;padding:8px 14px;}
      QDialog QTabBar::tab {background:#292929;color:white;padding:10px;}
      QDialog QTabBar::tab:selected {background:#283aa4;}
    )");
    QFile prefs(prefPath_);int page=0;if(prefs.open(QIODevice::ReadOnly)&&prefs.size()<1024*1024){const auto p=QJsonDocument::fromJson(prefs.readAll()).object();activeCategory_=p["category"].toString("Logos");page=qBound(0,p["page"].toInt(),2);}
    auto *root=new QVBoxLayout(this);root->setSizeConstraint(QLayout::SetNoConstraint);root->setContentsMargins(0,18,0,12);root->setSpacing(8);
    auto *header=new WrapLayout(8);logo_=new QLabel(this);logo_->setObjectName("jpLogo");logo_->setAlignment(Qt::AlignCenter);logo_->setAccessibleName("Logo oficial JP Fachina SYNC");header->addWidget(logo_);
    auto *headerSpacer=new QLabel(this);headerSpacer->setObjectName("jpHeaderSpacer");header->addWidget(headerSpacer);button("Cenas",header,this,[this]{scenesDialog();},"jpScenes");button("Editar\nposição",header,this,[this]{editPosition();},"jpEditPosition");button("Ad. Conteúdo",header,this,[this]{contentManager();},"jpContent");button("Take",header,this,[this]{run([this]{c_.take();});},"jpTake");button("Out",header,this,[this]{run([this]{c_.out();});},"jpOut");root->addLayout(header);
    pages_=new QStackedWidget(this);pages_->setObjectName("jpPages");pages_->setMinimumSize(0,0);pages_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);root->addWidget(pages_,1);const auto categories=panelCategories();
    for(int pageIndex=0;pageIndex<2;++pageIndex){
        auto *area=new QScrollArea(pages_);area->setWidgetResizable(true);auto *pageWidget=new QWidget;pageWidget->setStyleSheet("background:transparent;");auto *layout=new QVBoxLayout(pageWidget);layout->setContentsMargins(24,8,24,8);layout->setSpacing(12);
        for(int i=0;i<3;++i){const auto category=categories[pageIndex*3+i];auto *row=new QWidget(pageWidget);rows_[category]=row;auto *column=new QVBoxLayout(row);column->setContentsMargins(0,0,0,0);column->setSpacing(12);
            auto *titleRow=new QHBoxLayout;auto *title=new QLabel(category+":",row);headings_[category]=title;titleRow->addWidget(title,1);
            auto *previous=button("‹",titleRow,row,[this,category]{offsets_[category]=qMax(0,offsets_[category]-6);renderCards();},"jpPrevious_"+category);auto *next=button("›",titleRow,row,[this,category]{offsets_[category]+=6;renderCards();},"jpNext_"+category);for(auto *b:{previous,next}){b->setFixedSize(32,26);b->setStyleSheet("padding:0;border-radius:13px;");}column->addLayout(titleRow);
            auto *grid=new QGridLayout;grid->setContentsMargins(8,0,8,0);grid->setVerticalSpacing(12);grids_[category]=grid;
            for(int slot=0;slot<6;++slot){auto *card=new MediaCard(row);card->setObjectName(QString("jpCard_%1_%2").arg(category).arg(slot+1));cards_[category]<<card;grid->addWidget(card,0,slot);
                connect(card,&QAbstractButton::clicked,this,[this,card,category]{activeCategory_=category;if(card->mediaId().isEmpty()){contentManager();return;}run([this,card]{if(c_.selectedId==card->mediaId()||leaveDraft())c_.select(card->mediaId());});});
                card->setContextMenuPolicy(Qt::CustomContextMenu);connect(card,&QWidget::customContextMenuRequested,this,[this,card,category](const QPoint &point){if(card->mediaId().isEmpty())return;activeCategory_=category;run([this,card,point]{if(!leaveDraft())return;c_.select(card->mediaId());QMenu menu(this);auto *edit=menu.addAction("Editar conteúdo…");auto *position=menu.addAction("Editar posição…");auto *take=menu.addAction("Take");auto *out=menu.addAction("Out");take->setEnabled(c_.online);out->setEnabled(c_.online);const auto action=menu.exec(card->mapToGlobal(point));if(action==edit)contentManager();else if(action==position)editPosition();else if(action==take)c_.take();else if(action==out)c_.out();});});
            }column->addLayout(grid);layout->addWidget(row);if(i<2)layout->addStretch(1);
        }layout->addStretch(1);area->setWidget(pageWidget);pages_->addWidget(area);
    }
    scoreboardWorkspace();
    auto *footer=new QHBoxLayout;footer->setObjectName("jpFooter");footer->setContentsMargins(24,6,24,0);footer->setSpacing(12);tab1_=button("ABA 1",footer,this,[this]{showPage(0);},"jpTab1");tab2_=button("ABA 2",footer,this,[this]{showPage(1);},"jpTab2");tab3_=button("ABA 3",footer,this,[this]{showPage(2);},"jpTab3");footer->addStretch(1);
    quickLoop_=new QCheckBox("Reproduzir em loop",this);quickLoop_->setObjectName("jpQuickLoop");footer->addWidget(quickLoop_);connect(quickLoop_,&QCheckBox::toggled,this,[this](bool value){if(rendering_||c_.selected().isEmpty())return;run([this,value]{c_.edit({{"loop",value}});c_.save();});});button("Link OBS",footer,this,[this]{linkDialog();},"jpLink");root->addLayout(footer);
    auto *bottom=new QHBoxLayout;bottom->setContentsMargins(24,0,24,0);integration_=new QLabel(this);integration_->setObjectName("jpIntegration");bottom->addWidget(integration_);status_=new QLabel("Selecione um conteúdo · F11: tela cheia",this);status_->setObjectName("jpStatus");status_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);bottom->addWidget(status_,1);auto *view=button("⋯",bottom,this,[this]{viewMenu();},"jpView");view->setFixedSize(38,28);view->setToolTip("Tela cheia, monitor, projetos e sincronização");root->addLayout(bottom);
    // The scoreboard page scrolls internally; it must never enlarge the operator window.
    setMinimumSize(340,260);editorDialog_=new PositionDialog(c_,this);
    refreshTimer_=new QTimer(this);refreshTimer_->setSingleShot(true);refreshTimer_->setInterval(20);connect(refreshTimer_,&QTimer::timeout,this,&OperatorPanel::render);connect(&c,&Controller::changed,this,[this]{refreshTimer_->start();});connect(&c,&Controller::draftChanged,this,&OperatorPanel::renderDraft);connect(&c,&Controller::notice,this,[this](const QString &message,bool error){status_->setText(message);status_->setToolTip(message);status_->setStyleSheet(error?"color:#ff8589":"color:#e4e4e4");});showPage(page);render();
}
OperatorPanel::~OperatorPanel(){savePreferences();}
QString OperatorPanel::category()const{return activeCategory_;}
void OperatorPanel::setDisplayMode(const QString &mode){setProperty("displayMode",mode);}
void OperatorPanel::run(const std::function<void()> &action){try{action();}catch(const std::exception &e){c_.notify(QString::fromUtf8(e.what()),true);renderDraft();}}
bool OperatorPanel::leaveDraft(){if(!c_.dirty)return true;const auto answer=QMessageBox::question(this,"Preparação não salva","Salvar a preparação antes de continuar?",QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);if(answer==QMessageBox::Cancel)return false;if(answer==QMessageBox::Save)c_.save();else{c_.dirty=false;c_.draft=c_.selected()["draft"].toObject();}return true;}
void OperatorPanel::showPage(int index){pages_->setCurrentIndex(index);for(auto *tab:{tab1_,tab2_,tab3_}){tab->setProperty("active",tab==(index==0?tab1_:index==1?tab2_:tab3_));tab->style()->unpolish(tab);tab->style()->polish(tab);}const auto pageCategories=panelCategories().mid(index*3,3);if(!pageCategories.isEmpty()&&!pageCategories.contains(activeCategory_))activeCategory_=pageCategories.first();savePreferences();arrangeCards();}
void OperatorPanel::render(){
    const auto others=c_.otherProjectsOnAir();integration_->setText(c_.online?"● OBS conectado":"○ OBS desconectado");integration_->setToolTip(c_.obsStatus+(others.isEmpty()?QString():"\nProjetos ainda no ar: "+others.join(", ")));
    if(!others.isEmpty())integration_->setText(integration_->text()+" · no ar: "+others.join(", "));
    renderCards();renderScoreboardWorkspace();renderDraft();
}
void OperatorPanel::renderDraft(){
    rendering_=true;const auto m=c_.selected();quickLoop_->setVisible(m["mime"].toString().startsWith("video/"));quickLoop_->setChecked(c_.draft.value("loop").toBool(true));
    findChild<QPushButton*>("jpTake")->setEnabled(c_.online&&!m.isEmpty()&&!m["disabled"].toBool());findChild<QPushButton*>("jpOut")->setEnabled(c_.online&&!m.isEmpty()&&(c_.onAir(c_.selectedId)||m["live"].isObject()));rendering_=false;
}
QImage OperatorPanel::thumbnail(const QJsonObject &m){
    if(m["mime"]=="application/x-jp-scoreboard")try{return QImage(c_.store.scoreboardImagePath(c_.project(),m,true)).scaled(480,270,Qt::KeepAspectRatio,Qt::SmoothTransformation);}catch(...){return {};}
    if(m["mime"]=="application/x-jp-title")return titleImage(m["draft"].toObject()).scaled(480,270,Qt::KeepAspectRatio,Qt::SmoothTransformation);if(m["mime"].toString().startsWith("video/")&&!m.contains("thumbnail"))return {};
    try{const auto path=c_.store.mediaPath(m,m.contains("thumbnail"));const auto key=path+QString::number(QFileInfo(path).lastModified().toMSecsSinceEpoch());if(thumbnailCache_.contains(key))return thumbnailCache_[key];QImageReader reader(path);reader.setAutoTransform(true);const auto size=reader.size();if(size.isValid())reader.setScaledSize(size.scaled(480,270,Qt::KeepAspectRatio));const auto image=reader.read();if(thumbnailCache_.size()>120)thumbnailCache_.clear();thumbnailCache_[key]=image;return image;}catch(...){return {};}
}
void OperatorPanel::renderCards(){if(rendering_)return;for(const auto &category:panelCategories()){QList<QJsonObject> items;for(const auto &v:c_.project()["media"].toArray()){const auto m=v.toObject();if(panelCategory(m["category"].toString(),m["mime"].toString())==category)items<<m;}const int maxOffset=items.isEmpty()?0:(int(items.size())-1)/6*6;offsets_[category]=qBound(0,offsets_[category],maxOffset);const int offset=offsets_[category];auto *previous=findChild<QPushButton*>("jpPrevious_"+category);auto *next=findChild<QPushButton*>("jpNext_"+category);previous->setVisible(items.size()>6);next->setVisible(items.size()>6);previous->setEnabled(offset>0);next->setEnabled(offset+6<items.size());for(int i=0;i<6;++i){const auto m=items.value(offset+i);cards_[category][i]->content(m["id"].toString(),m["name"].toString(),thumbnail(m),m["mime"].toString().startsWith("video/"),!m.isEmpty()&&m["id"].toString()==c_.selectedId,!m.isEmpty()&&c_.onAir(m["id"].toString()),m["disabled"].toBool());}}}
void OperatorPanel::arrangeCards(){
    const int columns=width()>=900?6:width()>=540?3:2;const int margin=qBound(20,qRound(width()*.06),115);const int spacing=qBound(8,width()/76,24);findChild<QHBoxLayout*>("jpFooter")->setContentsMargins(margin,6,margin,0);
    for(const auto &category:panelCategories()){auto *grid=grids_[category];const int available=qMax(100,width()-2*margin-20);const int height=qRound((available-(columns-1)*spacing)/double(columns)*9./16);for(int i=0;i<6;++i){auto *card=cards_[category][i];grid->removeWidget(card);grid->addWidget(card,i/columns,i%columns);card->setFixedHeight(height);}grid->setHorizontalSpacing(spacing);for(int i=0;i<6;++i)grid->setColumnStretch(i,i<columns?1:0);auto *layout=qobject_cast<QVBoxLayout*>(rows_[category]->parentWidget()->layout());if(layout)layout->setContentsMargins(margin,8,margin,8);QFont font=headings_[category]->font();font.setPixelSize(qBound(20,width()/43,44));font.setBold(true);headings_[category]->setStyleSheet(QString("font-size:%1px;font-weight:700;").arg(font.pixelSize()));}
    const int buttonWidth=qBound(62,width()/12,170),buttonHeight=qBound(42,width()/27,72);for(const auto &name:{"jpScenes","jpEditPosition","jpContent","jpTake","jpOut"}){auto *b=findChild<QPushButton*>(name);b->setFixedSize(buttonWidth,buttonHeight);if(QString(name)=="jpContent")b->setText(width()<900?"Ad.\nConteúdo":"Ad. Conteúdo");QFont font=b->font();font.setPixelSize(qBound(13,width()/65,29));if(QString(name)=="jpEditPosition"||QString(name)=="jpContent")font.setPixelSize(qBound(12,width()/85,23));b->setStyleSheet(QString("font-size:%1px;font-weight:700;padding:0px 6px;").arg(font.pixelSize()));}
    logo_->setFixedSize(qBound(140,width()/5,400),buttonHeight);logo_->setPixmap(QPixmap(":/jp/assets/branding/jpfachina-sync-original.png").scaled(qMin(logo_->width()-28,250),buttonHeight-14,Qt::KeepAspectRatio,Qt::SmoothTransformation));auto *spacer=findChild<QLabel*>("jpHeaderSpacer");spacer->setFixedSize(qMax(0,width()-logo_->width()-buttonWidth*5-48-margin),buttonHeight);spacer->setVisible(width()>=1000);quickLoop_->setText(width()<420?"↻":width()<900?"Loop":"Reproduzir em loop");
    for(auto *tab:{tab1_,tab2_,tab3_,findChild<QPushButton*>("jpLink")}){tab->setFixedSize(buttonWidth,buttonHeight);QFont font=tab->font();font.setPixelSize(qBound(14,width()/60,30));tab->setStyleSheet(QString("font-size:%1px;font-weight:700;padding:0px 6px;").arg(font.pixelSize()));}
}
void OperatorPanel::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);arrangeCards();}
void OperatorPanel::savePreferences(){if(!pages_)return;QSaveFile f(prefPath_);const auto b=QJsonDocument(QJsonObject{{"version",3},{"category",activeCategory_},{"page",pages_->currentIndex()}}).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())c_.notify("Falha ao salvar preferências do painel.",true);}
void OperatorPanel::editPosition(){run([this]{const auto m=c_.selected();editorDialog_->openCategory(m["mime"]=="application/x-jp-scoreboard"?"Scoreboard":m.isEmpty()?category():panelCategory(m["category"].toString(),m["mime"].toString()));});}
void OperatorPanel::viewMenu(){QMenu menu(this);auto *full=menu.addAction("Tela cheia / janela · F11");auto *window=menu.addAction("Abrir janela de operação");auto *dock=menu.addAction("Acoplar ao OBS");auto *monitor=menu.addAction("Monitor e abertura automática…");menu.addSeparator();auto *sync=menu.addAction("Sincronizar posições…");auto *undo=menu.addAction("Desfazer sincronização");auto *redo=menu.addAction("Refazer sincronização");const auto a=menu.exec(QCursor::pos());if(a==full)emit fullscreenRequested();else if(a==window)emit windowRequested();else if(a==dock)emit dockRequested();else if(a==monitor)emit monitorRequested();else if(a==sync)synchronize();else if(a==undo)run([this]{c_.undo();});else if(a==redo)run([this]{c_.redo();});}
}
