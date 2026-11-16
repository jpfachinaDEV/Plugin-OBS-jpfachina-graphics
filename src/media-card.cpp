#include "media-card.hpp"
#include <QPainter>
#include <QPainterPath>

namespace jp {
MediaCard::MediaCard(QWidget *parent):QAbstractButton(parent){setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);}
void MediaCard::content(const QString &id,const QString &name,const QImage &image,bool video,bool selected,bool live,bool disabled){
    id_=id;name_=name;image_=image;video_=video;selected_=selected;live_=live;disabled_=disabled;
    setProperty("mediaId",id);setProperty("state",live?"live":selected?"selected":id.isEmpty()?"empty":"available");
    setAccessibleName(id.isEmpty()?"Adicionar conteúdo":name);setAccessibleDescription(live?"No ar no programa OBS":selected?"Selecionado para TAKE":id.isEmpty()?"Espaço vazio":"Disponível");
    setToolTip(id.isEmpty()?"Adicionar conteúdo nesta categoria":name+"\n"+accessibleDescription());update();
}
void MediaCard::paintEvent(QPaintEvent *){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);const QRectF area(2,2,width()-4,height()-4);
    p.fillRect(area,QColor("#141414"));
    if(!image_.isNull()){
        p.save();p.setClipRect(area.adjusted(2,2,-2,-2));const QSizeF size=QSizeF(image_.size()).scaled(area.size(),Qt::KeepAspectRatio);
        p.setRenderHint(QPainter::SmoothPixmapTransform);p.drawImage(QRectF(area.center()-QPointF(size.width()/2,size.height()/2),size),image_);p.restore();
    }else{
        p.setPen(QColor(id_.isEmpty()?"#606064":"#a6a6af"));QFont f=font();f.setPixelSize(qBound(12,width()/12,26));p.setFont(f);
        p.drawText(area,Qt::AlignCenter,id_.isEmpty()?"+":video_?"▶  PRÉVIA PENDENTE":"PRÉVIA INDISPONÍVEL");
    }
    if(!id_.isEmpty()){
        const int bar=qBound(18,height()/5,32);p.fillRect(QRectF(area.left(),area.bottom()-bar,area.width(),bar),QColor(0,0,0,190));
        QFont f=font();f.setPixelSize(qBound(10,width()/18,17));f.setBold(true);p.setFont(f);p.setPen(Qt::white);
        p.drawText(QRectF(area.left()+9,area.bottom()-bar,area.width()-18,bar),Qt::AlignVCenter,fontMetrics().elidedText(name_,Qt::ElideRight,int(area.width()-18)));
        if(live_||selected_||disabled_){const QString label=live_?"● NO AR":selected_?"SELECIONADO":"DESATIVADO";const QColor color=live_?QColor("#ff3035"):selected_?QColor("#69ed70"):QColor("#b3b3b3");
            f.setPixelSize(qBound(9,width()/22,14));p.setFont(f);const int badge=p.fontMetrics().horizontalAdvance(label)+14;p.fillRect(QRect(6,6,badge,21),QColor(0,0,0,210));p.setPen(color);p.drawText(QRect(13,6,badge,21),Qt::AlignVCenter,label);
        }
    }
    p.setPen(QPen(live_?QColor("#ff3035"):selected_?QColor("#69ed70"):QColor("#ffffff"),live_||selected_?5:3));p.setBrush(Qt::NoBrush);p.drawRect(area);
    if(hasFocus()){p.setPen(QPen(QColor("#56baff"),1,Qt::DashLine));p.drawRect(area.adjusted(6,6,-6,-6));}
}
}
