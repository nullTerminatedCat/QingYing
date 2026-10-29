#pragma once
#include <QColor>
#include <QIcon>
#include <QRectF>

class QPainter;

enum class Icon { Rect, Ellipse, Arrow, Pen, Text, Mosaic, Number, Undo, Redo, Pin, Save, Close, Confirm, Copy, Capture, Settings, Scroll, History, Clock, Rotate, Ocr, Translate, Qr, Picker, Record, Upload, Palette, Info, Folder };

// 24 栅格、1.5px 线宽、圆角端点的线性图标
void paintIcon(QPainter& p, Icon icon, const QRectF& r, const QColor& color);
QIcon appIcon();
QIcon menuIcon(Icon icon);
