#pragma once
#include "Icons.h"

#include <QColor>
#include <QList>
#include <QWidget>

// 悬浮圆角工具条（绘制自带阴影边距 Margin）
class ToolBar : public QWidget {
    Q_OBJECT
public:
    enum Id { TRect, TEllipse, TArrow, TPen, TText, TMosaic, TNumber, Undo, Redo, Pin, Save, Cancel, Confirm, LongShot, OcrBtn, QrBtn, UploadBtn };
    static constexpr int Margin = 0; // 阴影由父遮罩绘制，避免透明边缘吞掉点击
    explicit ToolBar(QWidget* parent);
    void setActive(int id);
    void setUndoRedo(bool canUndo, bool canRedo);
    QSize visualSize() const { return size() - QSize(2 * Margin, 2 * Margin); }

signals:
    void triggered(int id);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Item {
        int id;
        Icon icon;
        QString tip;
        QRect rect;
        bool separator;
    };
    int itemAt(const QPoint& p) const;
    bool isEnabled(int id) const;
    QList<Item> m_items;
    int m_hover = -1;
    int m_active = -1;
    bool m_canUndo = false, m_canRedo = false;
};

// 二级样式条：粗细 / 字号 + 颜色
class StyleBar : public QWidget {
    Q_OBJECT
public:
    static constexpr int Margin = 0;
    explicit StyleBar(QWidget* parent);
    void configure(bool textMode, bool showColors, int sizeIndex, const QColor& color);
    QSize visualSize() const { return size() - QSize(2 * Margin, 2 * Margin); }

signals:
    void sizeChanged(int index);
    void colorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    void relayout();
    bool m_text = false, m_colors = true;
    int m_size = 1;
    QColor m_color;
    QList<QRect> m_sizeRects, m_colorRects;
    QList<QColor> m_palette;
    int m_sepX = 0;
    int m_hover = -1;
};
