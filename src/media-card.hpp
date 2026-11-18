#pragma once
#include <QAbstractButton>
#include <QImage>

namespace jp {
class MediaCard : public QAbstractButton {
    Q_OBJECT
public:
    explicit MediaCard(QWidget *parent=nullptr);
    void content(const QString &id,const QString &name,const QImage &image,bool video,bool selected,bool live,bool disabled=false);
    QString mediaId()const{return id_;}
    bool hasHeightForWidth()const override{return true;}
    int heightForWidth(int width)const override{return qRound(width*9.0/16);}
    QSize sizeHint()const override{return {240,135};}
    QSize minimumSizeHint()const override{return {64,36};}
protected:
    void paintEvent(QPaintEvent *)override;
private:
    QString id_,name_;
    QImage image_;
    bool video_=false,selected_=false,live_=false,disabled_=false;
};
}
