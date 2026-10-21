#pragma once
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QSizeF>
#include <QString>

class QPainter;

enum class Tool { None, Rect, Ellipse, Arrow, Pen, Text, Mosaic, Number };

struct Annotation {
    Tool type = Tool::None;
    QColor color;
    qreal width = 3;
    QPointF p1, p2;
    QPolygonF points;
    QString text;
    int fontSize = 20;
    int number = 0;
    bool isValid() const;
};

QFont annotationFont(int pixelSize);
// canvas：逻辑坐标下整张截图的尺寸（马赛克源图按它铺满）
void paintAnnotation(QPainter& p, const Annotation& a, const QImage& mosaic, const QSizeF& canvas);
