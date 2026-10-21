#pragma once
#include <QImage>
#include <QRect>

class QScreen;

namespace ScreenGrabber {
// 以物理像素抓取桌面区域（GDI BitBlt，含分层窗口）
QImage grab(const QRect& physicalRect);
// 显示器的物理像素矩形
QRect monitorRect(QScreen* screen);
}
