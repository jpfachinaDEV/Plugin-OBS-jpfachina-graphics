#include "operator-panel.hpp"
#include <QDialog>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

namespace jp {
void OperatorPanel::synchronize(){run([this]{
    if(!leaveDraft())return;const auto pid=c_.projectId();if(pid.isEmpty())throw Error("Crie um projeto em Cenas.");
    QDialog dialog(this);dialog.setObjectName("jpSyncDialog");dialog.setWindowTitle("Sincronizar posições entre categorias");dialog.resize(660,620);auto *root=new QVBoxLayout(&dialog);root->addWidget(new QLabel("Origem · somente posições deste projeto",&dialog));auto *origin=new QComboBox(&dialog);origin->addItems(panelCategories());origin->setCurrentText(category());root->addWidget(origin);
    auto *properties=new QGridLayout;QList<QCheckBox*> checks;int index=0;
    for(const auto &field:QList<QPair<QString,QString>>{{"x","Horizontal · X"},{"y","Vertical · Y"},{"width","Largura"},{"height","Altura"},{"scale","Escala"},{"rotation","Rotação"},{"anchor","Âncora"},{"alignment","Alinhamento"},{"cropLeft","Recorte esquerdo"},{"cropRight","Recorte direito"},{"cropTop","Recorte superior"},{"cropBottom","Recorte inferior"}}){auto *check=new QCheckBox(field.second,&dialog);check->setProperty("field",field.first);check->setChecked(field.first=="x"||field.first=="y");properties->addWidget(check,index/3,index%3);++index;checks<<check;}root->addLayout(properties);root->addWidget(new QLabel("Destinos",&dialog));auto *targets=new QListWidget(&dialog);root->addWidget(targets,1);
    auto refresh=[&]{targets->clear();for(const auto &cat:panelCategories())if(cat!=origin->currentText()){auto *item=new QListWidgetItem(cat,targets);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(Qt::Unchecked);}};connect(origin,&QComboBox::currentTextChanged,&dialog,refresh);refresh();
    auto *note=new QLabel("A sincronização prepara as categorias selecionadas. Conteúdos, loop e posições já no ar são preservados. Desfazer e refazer ficam no menu ⋯.",&dialog);note->setWordWrap(true);root->addWidget(note);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Ok)->setText("Revisar sincronização…");root->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{run([&]{if(c_.projectId()!=pid)throw Error("O projeto ativo mudou. Reabra a sincronização.");QStringList selected,fields;for(int i=0;i<targets->count();++i)if(targets->item(i)->checkState()==Qt::Checked)selected<<targets->item(i)->text();for(auto *check:checks)if(check->isChecked())fields<<check->property("field").toString();if(selected.isEmpty()||fields.isEmpty())throw Error("Escolha destinos e propriedades.");const auto summary="Origem: "+origin->currentText()+"\nDestinos: "+selected.join(", ")+"\nCampos: "+fields.join(", ")+"\n\nAplicar somente à preparação?";if(QMessageBox::question(&dialog,"Revisar sincronização",summary,QMessageBox::Apply|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Apply)return;c_.syncCategories(origin->currentText(),selected,fields);dialog.accept();});});dialog.exec();
});}
}
