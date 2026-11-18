#pragma once
#include <QLayout>
#include <QWidget>
namespace jp {
class WrapLayout : public QLayout {
public:
    explicit WrapLayout(int spacing=6){setContentsMargins(0,0,0,0);setSpacing(spacing);}
    ~WrapLayout()override{while(auto *item=takeAt(0))delete item;}
    void addItem(QLayoutItem *item)override{items_.append(item);}
    int count()const override{return int(items_.size());}
    QLayoutItem *itemAt(int index)const override{return items_.value(index);}
    QLayoutItem *takeAt(int index)override{return index>=0&&index<items_.size()?items_.takeAt(index):nullptr;}
    Qt::Orientations expandingDirections()const override{return {};}
    bool hasHeightForWidth()const override{return true;}
    int heightForWidth(int width)const override{return arrange(QRect(0,0,width,0),false);}
    QSize sizeHint()const override{return minimumSize();}
    QSize minimumSize()const override{QSize result;for(auto *i:items_)result=result.expandedTo(i->minimumSize());return result;}
    void setGeometry(const QRect &rect)override{QLayout::setGeometry(rect);arrange(rect,true);}
private:
    QList<QLayoutItem*> items_;
    int arrange(const QRect &rect,bool apply)const{int x=rect.x(),y=rect.y(),line=0;for(auto *i:items_){if(i->isEmpty())continue;auto s=i->sizeHint();s.setWidth(qMin(s.width(),rect.width()));if(x>rect.x()&&x+s.width()>rect.right()+1){x=rect.x();y+=line+spacing();line=0;}if(apply)i->setGeometry(QRect(QPoint(x,y),s));x+=s.width()+spacing();line=qMax(line,s.height());}return y+line-rect.y();}
};
}
