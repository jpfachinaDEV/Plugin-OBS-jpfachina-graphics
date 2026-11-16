#pragma once
#include <QImage>
#include <QPainter>
#include <QJsonObject>
#include <QFont>
namespace jp {
inline QImage titleImage(const QJsonObject &layout){
    const int w=qBound(1,qRound(layout["width"].toDouble(1344)),4096),h=qBound(1,qRound(layout["height"].toDouble(173)),1024);
    QImage image(w,h,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(image.rect(),QColor(13,22,33,238));p.fillRect(0,0,qMax(4,w/130),h,QColor("#36d8b4"));
    const auto text=layout["text"].toObject();const int margin=qMax(12,w/40);QFont f("Segoe UI");f.setPixelSize(qMax(12,h/4));f.setBold(true);p.setFont(f);p.setPen(Qt::white);
    p.drawText(QRect(margin,h/9,w-margin*2,h*4/10),Qt::AlignVCenter|Qt::AlignLeft,p.fontMetrics().elidedText(text["name"].toString(),Qt::ElideRight,w-margin*2));
    f.setBold(false);f.setPixelSize(qMax(10,h/6));p.setFont(f);p.setPen(QColor("#b4c6d9"));
    const auto sub=text["role"].toString()+(text["city"].toString().isEmpty()?"":" · "+text["city"].toString());
    p.drawText(QRect(margin,h/2,w-margin*2,h*4/10),Qt::AlignVCenter|Qt::AlignLeft,p.fontMetrics().elidedText(sub,Qt::ElideRight,w-margin*2));return image;
}
inline QPointF anchorOffset(const QString &anchor,double width,double height){
    double x=anchor.contains("right")?1:anchor=="center"||anchor.endsWith("center")?.5:0;
    double y=anchor.startsWith("bottom")?1:anchor=="center"||anchor.startsWith("center")?.5:0;
    return QPointF(x*width,y*height);
}
}
