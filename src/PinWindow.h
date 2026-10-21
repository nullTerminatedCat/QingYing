#pragma once
#include <QImage>
#include <QWidget>

// 钉在桌面上的截图：拖动、滚轮缩放、Ctrl+滚轮调透明度、右键菜单、Esc 关闭
class PinWindow : public QWidget {
    Q_OBJECT
public:
    explicit PinWindow(const QImage& image);
    void placeImageAt(const QPoint& globalTopLeft);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    static constexpr int M = 14;
    QRectF imageRect() const;
    QRectF buttonRect(int i) const;
    int buttonAt(const QPointF& p) const;
    void updateSize();
    void copy();
    void save();

    QImage m_img;
    QSizeF m_base;
    qreal m_scale = 1;
    qreal m_opacity = 1;
    bool m_hover = false;
    int m_hoverBtn = -1;
    bool m_drag = false;
    QPoint m_dragOffset;
};
