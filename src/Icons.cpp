#include "Icons.h"
#include "Style.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

static void undoPath(QPainterPath& path)
{
    path.moveTo(9, 14);
    path.lineTo(5, 10);
    path.lineTo(9, 6);
    path.moveTo(5, 10);
    path.lineTo(15, 10);
    path.cubicTo(18, 10, 20, 12, 20, 15);
    path.cubicTo(20, 18, 18, 20, 15, 20);
    path.lineTo(11, 20);
}

void paintIcon(QPainter& p, Icon icon, const QRectF& r, const QColor& color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(r.topLeft());
    p.scale(r.width() / 24.0, r.height() / 24.0);
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    switch (icon) {
    case Icon::Rect: path.addRoundedRect(4, 5, 16, 14, 2, 2); break;
    case Icon::Ellipse: path.addEllipse(4, 5, 16, 14); break;
    case Icon::Arrow:
        path.moveTo(5, 19); path.lineTo(19, 5);
        path.moveTo(11, 5); path.lineTo(19, 5); path.lineTo(19, 13);
        break;
    case Icon::Pen:
        path.moveTo(4, 20); path.lineTo(8.5, 19); path.lineTo(19, 8.5); path.lineTo(15.5, 5);
        path.lineTo(5, 15.5); path.closeSubpath();
        path.moveTo(13.5, 7); path.lineTo(17, 10.5);
        break;
    case Icon::Text:
        path.moveTo(6, 6); path.lineTo(18, 6);
        path.moveTo(12, 6); path.lineTo(12, 19);
        path.moveTo(9.5, 19); path.lineTo(14.5, 19);
        break;
    case Icon::Mosaic: {
        path.addRoundedRect(4, 4, 16, 16, 2.5, 2.5);
        QColor fill = color;
        fill.setAlphaF(color.alphaF() * 0.85);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                if ((x + y) % 2 == 0) p.drawRect(QRectF(4 + x * 4, 4 + y * 4, 4, 4));
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        break;
    }
    case Icon::Number:
        path.addEllipse(4, 4, 16, 16);
        path.moveTo(10.5, 9.5); path.lineTo(12.5, 8.2); path.lineTo(12.5, 16);
        break;
    case Icon::Undo: undoPath(path); break;
    case Icon::Redo:
        p.translate(24, 0);
        p.scale(-1, 1);
        undoPath(path);
        break;
    case Icon::Pin:
        path.moveTo(14.5, 4); path.lineTo(20, 9.5); path.lineTo(16.5, 12); path.lineTo(14.5, 17);
        path.lineTo(7, 9.5); path.lineTo(12, 7.5); path.closeSubpath();
        path.moveTo(10.5, 13.5); path.lineTo(4.5, 19.5);
        break;
    case Icon::Save:
        path.moveTo(12, 4.5); path.lineTo(12, 15);
        path.moveTo(8, 11); path.lineTo(12, 15); path.lineTo(16, 11);
        path.moveTo(5, 15.5); path.lineTo(5, 19); path.lineTo(19, 19); path.lineTo(19, 15.5);
        break;
    case Icon::Close:
        path.moveTo(6.5, 6.5); path.lineTo(17.5, 17.5);
        path.moveTo(17.5, 6.5); path.lineTo(6.5, 17.5);
        break;
    case Icon::Confirm:
        path.moveTo(5, 12.5); path.lineTo(10, 17.5); path.lineTo(19, 7);
        break;
    case Icon::Copy:
        path.addRoundedRect(8.5, 8.5, 11.5, 11.5, 2, 2);
        path.moveTo(15.5, 8.5); path.lineTo(15.5, 4); path.lineTo(4, 4); path.lineTo(4, 15.5); path.lineTo(8.5, 15.5);
        break;
    case Icon::Capture:
        path.moveTo(4, 9); path.lineTo(4, 4); path.lineTo(9, 4);
        path.moveTo(15, 4); path.lineTo(20, 4); path.lineTo(20, 9);
        path.moveTo(20, 15); path.lineTo(20, 20); path.lineTo(15, 20);
        path.moveTo(9, 20); path.lineTo(4, 20); path.lineTo(4, 15);
        break;
    case Icon::Settings:
        path.addEllipse(QPointF(12, 12), 3, 3);
        path.addEllipse(QPointF(12, 12), 8, 8);
        break;
    }
    p.drawPath(path);
    p.restore();
}

QIcon appIcon()
{
    QIcon icon;
    for (int s : {16, 20, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(s, s);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        QLinearGradient g(0, 0, s, s);
        g.setColorAt(0, QColor(0x6A, 0x83, 0xFF));
        g.setColorAt(1, Style::AccentHover);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(0, 0, s, s), s * 0.24, s * 0.24);
        const qreal m = s * 0.14;
        QPainter& q = p;
        q.save();
        QPen pen(Qt::white, qMax(1.5, s / 12.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        q.setPen(pen);
        const qreal a = m + s * 0.08, b = s - a, l = s * 0.2;
        QPainterPath path;
        path.moveTo(a, a + l); path.lineTo(a, a); path.lineTo(a + l, a);
        path.moveTo(b - l, a); path.lineTo(b, a); path.lineTo(b, a + l);
        path.moveTo(b, b - l); path.lineTo(b, b); path.lineTo(b - l, b);
        path.moveTo(a + l, b); path.lineTo(a, b); path.lineTo(a, b - l);
        q.drawPath(path);
        q.setPen(Qt::NoPen);
        q.setBrush(Qt::white);
        q.drawEllipse(QPointF(s / 2.0, s / 2.0), s * 0.09, s * 0.09);
        q.restore();
        icon.addPixmap(pm);
    }
    return icon;
}

QIcon menuIcon(Icon i)
{
    QPixmap pm(40, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paintIcon(p, i, QRectF(4, 4, 32, 32), Style::Text);
    p.end();
    return QIcon(pm);
}
