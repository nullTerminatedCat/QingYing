#include "Style.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

namespace Style {
QList<QColor> annotationColors()
{
    return {QColor(0xFF, 0x45, 0x3A), QColor(0xFF, 0x9F, 0x0A), QColor(0xFF, 0xD6, 0x0A),
            QColor(0x32, 0xD7, 0x4B), QColor(0x4F, 0x6B, 0xFF), QColor(0xBF, 0x5A, 0xF2),
            QColor(0x1C, 0x1C, 0x1E), QColor(0xFF, 0xFF, 0xFF)};
}

QString uiFontFamily() { return QStringLiteral("Microsoft YaHei UI"); }

void paintShadow(QPainter& p, const QRectF& r, qreal radius, int spread)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    for (int i = spread; i >= 1; --i) {
        const qreal t = 1.0 - qreal(i) / spread;
        p.setBrush(QColor(20, 24, 40, int(2 + 10 * t * t)));
        p.drawRoundedRect(r.adjusted(-i, -i + 3, i, i + 3), radius + i, radius + i);
    }
    p.restore();
}

void paintGlass(QPainter& p, const QRectF& r, qreal radius)
{
    paintShadow(p, r, radius);
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0, 0, 0, 18), 1));
    p.setBrush(Glass);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
}

static QFont pillFont()
{
    QFont f(uiFontFamily());
    f.setPixelSize(12);
    return f;
}

QSizeF pillSize(const QString& text)
{
    QFontMetricsF fm(pillFont());
    return {fm.horizontalAdvance(text) + 16, fm.height() + 8};
}

QRectF paintPill(QPainter& p, const QPointF& topLeft, const QString& text)
{
    const QRectF r(topLeft, pillSize(text));
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Pill);
    p.drawRoundedRect(r, 8, 8);
    p.setFont(pillFont());
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
    return r;
}
}
