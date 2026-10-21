#pragma once
#include <QList>
#include <QPoint>
#include <QRect>

// 截图开始前按 Z 序快照所有可见窗口及其子控件（物理像素坐标）
class WindowDetector {
public:
    void snapshot();
    // controls=true 时优先返回光标下最小的子控件
    QRect hitTest(const QPoint& physical, bool controls) const;

private:
    struct Entry {
        QRect rect;
        QList<QRect> children; // 面积升序
    };
    QList<Entry> m_windows; // Z 序，最上层在前
};
