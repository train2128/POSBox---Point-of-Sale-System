#include "LineIcons.h"
#include "StyleManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {

static void draw(QPainter& p,const QString& n,const QRectF& r,const QColor& c,qreal w){ p.save(); p.translate(r.topLeft()); p.scale(r.width()/24.0,r.height()/24.0); QPen pen(c,w*24.0/qMax(14.0,r.width())*1.0); pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin); p.setPen(pen); p.setBrush(Qt::NoBrush);
 auto line=[&](qreal a,qreal b,qreal x,qreal y){p.drawLine(QPointF(a,b),QPointF(x,y));}; auto rr=[&](qreal a,qreal b,qreal x,qreal y,qreal ra=2){p.drawRoundedRect(QRectF(a,b,x,y),ra,ra);};
 if(n=="store"){ line(3,9,21,9); p.drawPolyline(QPolygonF{QPointF(4,9),QPointF(6,3),QPointF(18,3),QPointF(20,9)}); rr(5,9,14,12,1); rr(10,14,4,7,0.5);}
 else if(n=="users"){ p.drawEllipse(QPointF(9,8),4,4); p.drawArc(QRectF(2,14,14,14),30*16,120*16); p.drawEllipse(QPointF(17,9),3,3); p.drawArc(QRectF(13,15,10,10),30*16,100*16);}
 else if(n=="user"){ p.drawEllipse(QPointF(12,8),4.5,4.5); p.drawArc(QRectF(4,14,16,16),25*16,130*16);}
 else if(n=="palette"){ p.drawEllipse(QPointF(12,12),9,9); for(auto q:{QPointF(8,9),QPointF(13,7),QPointF(16.5,11)}) {p.save();p.setBrush(c);p.drawEllipse(q,1,1);p.restore();} p.drawArc(QRectF(9,13,6,6),0,360*16);}
 else if(n=="folder"){ p.drawPolyline(QPolygonF{QPointF(3,19),QPointF(3,6),QPointF(9,6),QPointF(11,9),QPointF(21,9),QPointF(21,19),QPointF(3,19)});}
 else if(n=="db"){ p.drawEllipse(QRectF(4,3,16,6)); line(4,6,4,18); line(20,6,20,18); p.drawArc(QRectF(4,15,16,6),180*16,180*16); p.drawArc(QRectF(4,9,16,6),180*16,180*16);}
 else if(n=="printer"){ rr(7,3,10,5,1); rr(3,8,18,9,2); rr(7,14,10,7,1);}
 else if(n=="bell"){ p.drawArc(QRectF(6,3,12,14),0,180*16); line(6,10,6,16); line(18,10,18,16); line(3.5,17,20.5,17); p.drawArc(QRectF(10,17,4,4),180*16,180*16);}
 else if(n=="logout"){ p.drawPolyline(QPolygonF{QPointF(10,4),QPointF(4,4),QPointF(4,20),QPointF(10,20)}); line(10,12,21,12); p.drawPolyline(QPolygonF{QPointF(17,8),QPointF(21,12),QPointF(17,16)});}
 else if(n=="search"){ p.drawEllipse(QPointF(10.5,10.5),6.5,6.5); line(15.5,15.5,21,21);}
 else if(n=="edit"){ p.drawPolyline(QPolygonF{QPointF(4,20),QPointF(4.5,16),QPointF(16,4.5),QPointF(19.5,8),QPointF(8,19.5),QPointF(4,20)});}
 else if(n=="undo"){ p.drawPolyline(QPolygonF{QPointF(8,4),QPointF(3,9),QPointF(8,14)}); p.drawArc(QRectF(6,9-0,15,11),0*16,90*16); line(3,9,13.5,9); p.drawArc(QRectF(6,9,15,11),270*16,90*16); line(13.5,20,7,20);}
 else if(n=="trash"){ line(4,6,20,6); p.drawPolyline(QPolygonF{QPointF(9,6),QPointF(9,3),QPointF(15,3),QPointF(15,6)}); p.drawPolyline(QPolygonF{QPointF(6,6),QPointF(7,21),QPointF(17,21),QPointF(18,6)});}
 else if(n=="cal"){ rr(3,5,18,16,2); line(3,10,21,10); line(8,3,8,7); line(16,3,16,7);}
 else if(n=="chevd") p.drawPolyline(QPolygonF{QPointF(5,9),QPointF(12,16),QPointF(19,9)});
 else if(n=="chevl") p.drawPolyline(QPolygonF{QPointF(15,5),QPointF(8,12),QPointF(15,19)});
 else if(n=="chevr") p.drawPolyline(QPolygonF{QPointF(9,5),QPointF(16,12),QPointF(9,19)});
 else if(n=="tag"){ p.drawPolyline(QPolygonF{QPointF(3,12),QPointF(3,3),QPointF(12,3),QPointF(21,12),QPointF(12,21),QPointF(3,12)}); p.save();p.setBrush(c);p.drawEllipse(QPointF(7.5,7.5),1,1);p.restore();}
 else if(n=="plus"){ line(12,4,12,20); line(4,12,20,12);}
 else if(n=="cloud"){ p.drawArc(QRectF(3,10,9,9),90*16,180*16); p.drawArc(QRectF(7,5,11,11),20*16,160*16); p.drawArc(QRectF(13,10,8,9),270*16,180*16); line(7.5,19,17,19);}
 else if(n=="clock"){ p.drawEllipse(QPointF(12,12),9,9); p.drawPolyline(QPolygonF{QPointF(12,6.5),QPointF(12,12),QPointF(16,14)});}
 else if(n=="receipt"){ p.drawPolyline(QPolygonF{QPointF(5,3),QPointF(19,3),QPointF(19,21),QPointF(16.5,19),QPointF(14,21),QPointF(12,19),QPointF(10,21),QPointF(7.5,19),QPointF(5,21),QPointF(5,3)}); line(8.5,8,15.5,8); line(8.5,12,15.5,12);}
 else if(n=="check") p.drawPolyline(QPolygonF{QPointF(4,12.5),QPointF(10,18.5),QPointF(20,6)});
 else if(n=="x"){ line(6,6,18,18); line(18,6,6,18);}
 else if(n=="cash"){ rr(3,7,18,11,2); p.drawEllipse(QPointF(12,12.5),2.5,2.5);}
 else if(n=="download"){ line(12,3,12,15); p.drawPolyline(QPolygonF{QPointF(7,11),QPointF(12,16),QPointF(17,11)}); line(4,20,20,20);}
 else if(n=="image"){ rr(3,4,18,16,2); p.drawEllipse(QPointF(8.5,9.5),1.5,1.5); p.drawPolyline(QPolygonF{QPointF(3,17),QPointF(9,12),QPointF(13,16),QPointF(16,13),QPointF(21,18)});}
 else if(n=="refresh"){ p.drawArc(QRectF(4,4,16,16),40*16,270*16); p.drawPolyline(QPolygonF{QPointF(20,3),QPointF(19.5,9),QPointF(14,8)});}
 else if(n=="grid"){ for(auto q:{QPointF(4,4),QPointF(14,4),QPointF(4,14),QPointF(14,14)}) rr(q.x(),q.y(),6,6,1.5);}
 else if(n=="cup"){ p.drawPolyline(QPolygonF{QPointF(5,8),QPointF(6.5,21),QPointF(15.5,21),QPointF(17,8),QPointF(5,8)}); p.drawArc(QRectF(14,10,7,7),270*16,180*16); line(9,3,9,5); line(13,3,13,5);}
 else if(n=="food"){ p.drawArc(QRectF(3,4,18,16),0,180*16); line(3,12,21,12); rr(4,14,16,5,2);}
 else if(n=="wrench"){ line(5,19,13,11); p.drawArc(QRectF(11,3,10,10),200*16,290*16);}
 else if(n=="eye"){ QPainterPath e; e.moveTo(2,12); e.quadTo(12,2,22,12); e.quadTo(12,22,2,12); p.drawPath(e); p.drawEllipse(QPointF(12,12),3,3);}
 else if(n=="eyeoff"){ QPainterPath e; e.moveTo(2,12); e.quadTo(12,2,22,12); e.quadTo(12,22,2,12); p.drawPath(e); line(4,4,20,20);}
 else if(n=="alert"){ line(12,4,12,14); p.drawEllipse(QPointF(12,19),0.6,0.6);}
 else if(n=="question"){ p.drawArc(QRectF(7,3,10,10),-60*16,240*16); line(14.5,12,12,15); p.drawEllipse(QPointF(12,20),0.6,0.6);}
 else if(n=="box"){ p.drawPolyline(QPolygonF{QPointF(3,8),QPointF(12,3),QPointF(21,8),QPointF(21,17),QPointF(12,22),QPointF(3,17),QPointF(3,8)}); line(3,8,12,13); line(21,8,12,13); line(12,13,12,22);}
 p.restore(); }

} // namespace

namespace LineIcons {

Colors colors()
{
    if (StyleManager::loadColorScheme() == StyleManager::ColorScheme::Light)
        return {QColor("#40331F"), QColor("#8B7B62"), QColor("#C8894A")};
    return {QColor("#E6EAF0"), QColor("#94A0B3"), QColor("#4A90E2")};
}

QPixmap pixmap(const QString& name, const QColor& color, int size)
{
    constexpr qreal Scale = 3.0;  // stays crisp on high-DPI screens
    QPixmap px(QSize(size, size) * Scale);
    px.setDevicePixelRatio(Scale);
    px.fill(Qt::transparent);
    QPainter painter(&px);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, name, QRectF(1, 1, size - 2, size - 2), color, 1.5);
    painter.end();
    return px;
}

QIcon icon(const QString& name, const QColor& color, int size)
{
    return QIcon(pixmap(name, color, size));
}

QIcon navIcon(const QString& name, const QColor& normal, const QColor& active, int size)
{
    QIcon result;
    result.addPixmap(pixmap(name, normal, size), QIcon::Normal, QIcon::Off);
    result.addPixmap(pixmap(name, active, size), QIcon::Normal, QIcon::On);
    return result;
}

} // namespace LineIcons
