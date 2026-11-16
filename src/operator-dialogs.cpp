#include "operator-panel.hpp"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QDesktopServices>
#include <QSpinBox>
#include <QGridLayout>
#include <QUrl>
#include <QTimer>
#include <QDialogButtonBox>

namespace jp {
void OperatorPanel::contentManager(){run([this]{
    if(!leaveDraft())return;QDialog dialog(this);dialog.setObjectName("jpContentDialog");dialog.setWindowTitle("Adicionar e editar conteúdo");dialog.resize(760,580);auto *root=new QVBoxLayout(&dialog);auto *cat=new QComboBox(&dialog);cat->setObjectName("jpContentCategory");for(const auto &v:c_.project()["categories"].toArray())cat->addItem(v.toString());cat->setCurrentText(category());root->addWidget(cat);auto *list=new QListWidget(&dialog);list->setObjectName("jpContentList");root->addWidget(list,1);
    auto refresh=[&]{const auto selected=c_.selectedId;list->clear();for(const auto &v:c_.project()["media"].toArray()){const auto m=v.toObject();if(m["category"].toString()!=cat->currentText())continue;auto *item=new QListWidgetItem(m["name"].toString()+(c_.onAir(m["id"].toString())?" · NO AR":""),list);item->setData(Qt::UserRole,m["id"]);item->setToolTip(m["origin"].toString());if(m["id"].toString()==selected)list->setCurrentItem(item);}};
    connect(cat,&QComboBox::currentTextChanged,&dialog,[&]{activeCategory_=cat->currentText();refresh();});connect(list,&QListWidget::currentItemChanged,&dialog,[&](QListWidgetItem *item){if(item)c_.select(item->data(Qt::UserRole).toString());});connect(&c_.store,&Store::changed,&dialog,refresh);
    auto add=[&](const QString &label,const QString &object,const std::function<void()> &action){auto *b=new QPushButton(label,&dialog);b->setObjectName(object);root->addWidget(b);connect(b,&QPushButton::clicked,&dialog,[this,action]{run(action);});};
    add("Adicionar arquivos locais…","jpImport",[&]{activeCategory_=cat->currentText();importMedia();});add("Adicionar URL HTTPS direta…","jpDownload",[&]{activeCategory_=cat->currentText();download();});
    add("Editar conteúdo…","jpEditContent",[&]{if(!list->currentItem())throw Error("Selecione um conteúdo cadastrado.");contentProperties();});
    add("Substituir arquivo…","jpReplaceContent",[&]{if(!list->currentItem())throw Error("Selecione um conteúdo cadastrado.");if(c_.onAir(c_.selectedId)||c_.selected()["live"].isObject())throw Error("Execute OUT e aguarde a confirmação antes de substituir.");const auto file=QFileDialog::getOpenFileName(&dialog,"Substituir arquivo sem alterar posição",{},"Mídias (*.png *.jpg *.jpeg *.webp *.gif *.webm *.mp4)");if(!file.isEmpty())c_.store.replaceMediaAsync(c_.projectId(),c_.selectedId,file);});
    add("Excluir cadastro selecionado…","jpRemoveContent",[&]{if(!list->currentItem())throw Error("Selecione um conteúdo cadastrado.");if(!c_.online)throw Error("Conecte ao OBS para verificar se o conteúdo está no ar.");if(c_.onAir(c_.selectedId)||c_.selected()["live"].isObject())throw Error("Execute OUT e aguarde a confirmação antes de excluir.");if(QMessageBox::question(&dialog,"Excluir cadastro","Excluir somente o cadastro? O arquivo original será preservado.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)c_.store.removeMedia(c_.projectId(),c_.selectedId);});
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);root->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);refresh();dialog.exec();
});}
void OperatorPanel::importMedia(){run([this]{const auto pid=c_.projectId(),cat=category();if(pid.isEmpty()||cat.isEmpty())throw Error("Crie um projeto e selecione a categoria.");
    const auto files=QFileDialog::getOpenFileNames(this,"Importar na categoria "+cat,{},"Mídias (*.png *.jpg *.jpeg *.webp *.gif *.webm *.mp4)");
    for(const auto &path:files)c_.store.importFileAsync(pid,path,cat);if(!files.isEmpty())c_.notify("Importação iniciada em segundo plano.");
});}
void OperatorPanel::download(){run([this]{bool ok=false;const auto url=QInputDialog::getText(this,"Adicionar URL HTTPS","URL pública direta (máximo 256 MB; sem redirecionamentos)",QLineEdit::Normal,{},&ok);if(!ok||url.isEmpty())return;
    const QUrl parsed(url,QUrl::StrictMode);if(parsed.scheme()!="https"||!parsed.isValid())throw Error("Use uma URL HTTPS válida.");c_.downloader.fetch(c_.projectId(),url,category());c_.notify("Download iniciado. A mídia ficará disponível no cache local.");});}
}
