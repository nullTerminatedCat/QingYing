#pragma once
#include <QColor>
#include <QList>
#include <QRectF>
#include <QString>

class QPainter;

namespace Style {
inline const QColor Accent{0x4F, 0x6B, 0xFF};
inline const QColor AccentHover{0x3D, 0x57, 0xE8};
inline const QColor AccentTint{79, 107, 255, 31};
inline const QColor Text{0x1C, 0x1C, 0x1E};
inline const QColor Text2{0x6E, 0x6E, 0x73};
inline const QColor Text3{0xA1, 0xA1, 0xA6};
inline const QColor Line{0, 0, 0, 15};
inline const QColor Glass{255, 255, 255, 236};
inline const QColor Overlay{15, 18, 30, 107};
inline const QColor Pill{28, 28, 30, 184};
inline const QColor Danger{0xFF, 0x45, 0x3A};

QList<QColor> annotationColors();
QString uiFontFamily();
// 柔和阴影 + 圆角背景
void paintShadow(QPainter& p, const QRectF& r, qreal radius, int spread = 10);
void paintGlass(QPainter& p, const QRectF& r, qreal radius, bool shadow = true);
// 深色胶囊标签，返回绘制区域
QRectF paintPill(QPainter& p, const QPointF& topLeft, const QString& text);
QSizeF pillSize(const QString& text);
}
