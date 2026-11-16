#include "position-editor.hpp"
#include "title-image.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QImageReader>
#include <QFileInfo>
#include <QHideEvent>
#include <algorithm>
namespace jp {
PositionCanvas::PositionCanvas(PositionSession &session,QWidget *parent):QWidget(parent),session_(session){setMinimumSize(320,180);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setMouseTracking(true);connect(&session_,&PositionSession::changed,this,qOverload<>(&QWidget::update));}
QRectF PositionCanvas::compositionRect()const{const double w=qMax(1,session_.frame().width()),h=qMax(1,session_.frame().height());const double s=std::min(width()/w,height()/h);return QRectF((width()-w*s)/2,(height()-h*s)/2,w*s,h*s);}
void PositionCanvas::paintEvent(QPaintEvent*){
    QPainter p(this);p.fillRect(rect(),QColor("#090d13"));const auto bounds=compositionRect();p.fillRect(bounds,Qt::black);const double w=qMax(1,session_.frame().width()),h=qMax(1,session_.frame().height());p.translate(bounds.topLeft());p.scale(bounds.width()/w,bounds.height()/h);
    p.setPen(QPen(QColor("#243041"),1));for(int i=1;i<12;++i)p.drawLine(QPointF(w*i/12,0),QPointF(w*i/12,h));for(int i=1;i<8;++i)p.drawLine(QPointF(0,h*i/8),QPointF(w,h*i/8));
    p.setPen(QPen(QColor("#415264"),1,Qt::DashLine));p.drawRect(QRectF(w*.05,h*.05,w*.9,h*.9));
    const auto m=session_.media();if(session_.category().isEmpty()){p.resetTransform();p.setPen(QColor("#9daec3"));p.drawText(rect(),Qt::AlignCenter,"Selecione uma peça para posicionar");return;}
    const auto d=session_.layout();const double scale=d["scale"].toDouble(1),iw=d["width"].toDouble(),ih=d["height"].toDouble();
    const QImage image=session_.image();
    const double left=d["cropLeft"].toDouble(),top=d["cropTop"].toDouble(),cw=iw-left-d["cropRight"].toDouble(),ch=ih-top-d["cropBottom"].toDouble();
    p.translate(d["x"].toDouble(),d["y"].toDouble());p.rotate(d["rotation"].toDouble());p.scale(scale,scale);const auto offset=anchorOffset(d["anchor"].toString("top-left"),cw,ch);p.translate(-offset);p.setOpacity(d["opacity"].toDouble(100)/100);
    QRectF art(0,0,cw,ch);if(!image.isNull()){const QRectF source(image.width()*left/iw,image.height()*top/ih,image.width()*cw/iw,image.height()*ch/ih);const auto fitted=source.size().scaled(art.size(),Qt::KeepAspectRatio);const auto padding=anchorOffset(d["alignment"].toString("center"),cw-fitted.width(),ch-fitted.height());p.drawImage(QRectF(padding,fitted),image,source);}else{p.fillRect(art,QColor("#25364b"));p.setPen(Qt::white);QFont f=p.font();f.setPixelSize(qMax(12,int(ih/6)));p.setFont(f);p.drawText(art,Qt::AlignCenter,m["mime"].toString().startsWith("video/")?"VÍDEO · miniatura local":m["name"].toString(session_.category()));}
    p.setOpacity(1);p.setPen(QPen(QColor("#36d8b4"),2*w/bounds.width()/scale));p.drawRect(art);const double handle=13*w/bounds.width()/scale;p.fillRect(QRectF(cw-handle,ch-handle,handle*2,handle*2),QColor("#36d8b4"));
}
void PositionCanvas::mousePressEvent(QMouseEvent *e){if(e->button()!=Qt::LeftButton||session_.category().isEmpty())return;const auto b=compositionRect();if(!b.contains(e->position()))return;
    const double s=b.width()/double(qMax(1,session_.frame().width()));startPoint_=(e->position()-b.topLeft())/s;start_=session_.layout();
    const double cw=start_.value("width").toDouble()-start_.value("cropLeft").toDouble()-start_.value("cropRight").toDouble(),ch=start_.value("height").toDouble()-start_.value("cropTop").toDouble()-start_.value("cropBottom").toDouble();
    QTransform transform;transform.translate(start_.value("x").toDouble(),start_.value("y").toDouble());transform.rotate(start_.value("rotation").toDouble());transform.scale(start_.value("scale").toDouble(1),start_.value("scale").toDouble(1));const auto a=anchorOffset(start_.value("anchor").toString("top-left"),cw,ch);transform.translate(-a.x(),-a.y());
    const auto local=transform.inverted().map(startPoint_);const QRectF r(0,0,cw,ch);resizing_=QLineF(local,r.bottomRight()).length()<16/s;dragging_=r.contains(local)||resizing_;if(dragging_)grabMouse();
}
void PositionCanvas::mouseMoveEvent(QMouseEvent *e){if(!dragging_)return;const auto b=compositionRect();const double s=b.width()/double(qMax(1,session_.frame().width()));const auto now=(e->position()-b.topLeft())/s;const auto delta=now-startPoint_;
    try{
        if(resizing_){
            QTransform inverse;inverse.rotate(-start_.value("rotation").toDouble());
            const auto movement=inverse.map(delta)/start_.value("scale").toDouble(1);
            const double cropX=start_.value("cropLeft").toDouble()+start_.value("cropRight").toDouble();
            const double cropY=start_.value("cropTop").toDouble()+start_.value("cropBottom").toDouble();
            const double w=qBound(cropX+1,start_.value("width").toDouble()+movement.x(),16384.0);
            const double rawHeight=e->modifiers().testFlag(Qt::ShiftModifier)?w*start_.value("height").toDouble()/start_.value("width").toDouble():start_.value("height").toDouble()+movement.y();
            const double h=qBound(cropY+1,rawHeight,16384.0);
            const auto anchor=start_.value("anchor").toString("top-left");
            const auto previous=anchorOffset(anchor,start_.value("width").toDouble()-cropX,start_.value("height").toDouble()-cropY);
            const auto next=anchorOffset(anchor,w-cropX,h-cropY);
            QTransform transform;transform.rotate(start_.value("rotation").toDouble());transform.scale(start_.value("scale").toDouble(1),start_.value("scale").toDouble(1));
            const auto correction=transform.map(next-previous);
            session_.edit({{"width",w},{"height",h},{"x",start_.value("x").toDouble()+correction.x()},{"y",start_.value("y").toDouble()+correction.y()}});
        }else session_.edit({{"x",qBound(-10000.0,start_.value("x").toDouble()+delta.x(),10000.0)},{"y",qBound(-10000.0,start_.value("y").toDouble()+delta.y(),10000.0)}});
    }catch(const std::exception &ex){Q_UNUSED(ex);}
}
void PositionCanvas::mouseReleaseEvent(QMouseEvent*){if(dragging_)releaseMouse();dragging_=false;}
void PositionCanvas::hideEvent(QHideEvent *event){if(dragging_&&mouseGrabber()==this)releaseMouse();dragging_=false;resizing_=false;QWidget::hideEvent(event);}
}
