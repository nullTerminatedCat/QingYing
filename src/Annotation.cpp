#include "Annotation.h"
#include "Style.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

bool Annotation::isValid() const
{
    switch (type) {
    case Tool::Rect:
    case Tool::Ellipse:
    case Tool::Arrow: return QLineF(p1, p2).length() >= 3;
    case Tool::Pen:
    case Tool::Mosaic: return !points.isEmpty();
    case Tool::Text: return !text.trimmed().isEmpty();
    case Tool::Number: return true;
    default: return false;
    }
}

QFont annotationFont(int px)
{
    QFont f(Style::uiFontFamily());
    f.setPixelSize(px);
    return f;
}

void paintAnnotation(QPainter& p, const Annotation& a, const QImage& mosaic, const QSizeF& canvas)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(a.color, a.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    switch (a.type) {
    case Tool::Rect:
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(a.p1, a.p2).normalized());
        break;
    case Tool::Ellipse:
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(a.p1, a.p2).normalized());
        break;
    case Tool::Arrow: {
        const QLineF line(a.p1, a.p2);
        const qreal head = qMin(12 + a.width * 2.5, line.length());
        QLineF back(a.p2, a.p1);
        back.setLength(head);
        const QPointF base = back.p2();
        const QPointF d = a.p2 - base;
        const QPointF n(-d.y() * 0.5, d.x() * 0.5);
        p.setPen(pen);
        p.drawLine(a.p1, base + (a.p2 - base) * 0.2);
        p.setPen(Qt::NoPen);
        p.setBrush(a.color);
        p.drawPolygon(QPolygonF{a.p2, base + n, base - n});
        break;
    }
    case Tool::Pen:
        p.setPen(pen);
        if (a.points.size() == 1) p.drawPoint(a.points.first());
        else p.drawPolyline(a.points);
        break;
    case Tool::Mosaic: {
        if (a.points.isEmpty()) break;
        QPainterPath path(a.points.first());
        for (int i = 1; i < a.points.size(); ++i) path.lineTo(a.points[i]);
        QPainterPathStroker s;
        s.setWidth(a.width);
        s.setCapStyle(Qt::RoundCap);
        s.setJoinStyle(Qt::RoundJoin);
        QPainterPath area = s.createStroke(path);
        area.addEllipse(a.points.first(), a.width / 2, a.width / 2);
        area.setFillRule(Qt::WindingFill);
        p.setClipPath(area, Qt::IntersectClip);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.drawImage(QRectF(QPointF(0, 0), canvas), mosaic);
        break;
    }
    case Tool::Text: {
        const QFont f = annotationFont(a.fontSize);
        p.setFont(f);
        p.setPen(a.color);
        QFontMetricsF fm(f);
        qreal y = a.p1.y();
        for (const QString& line : a.text.split(QLatin1Char('\n'))) {
            p.drawText(QPointF(a.p1.x(), y + fm.ascent()), line);
            y += fm.lineSpacing();
        }
        break;
    }
    case Tool::Number: {
        const qreal r = 10 + a.width * 1.5;
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(a.color);
        p.drawEllipse(a.p1, r, r);
        QFont f = annotationFont(int(r * 1.1));
        f.setBold(true);
        p.setFont(f);
        p.setPen(a.color.lightness() > 200 ? Style::Text : QColor(Qt::white));
        p.drawText(QRectF(a.p1.x() - r, a.p1.y() - r, 2 * r, 2 * r), Qt::AlignCenter, QString::number(a.number));
        break;
    }
    default: break;
    }
    p.restore();
}
