#include "ToolBars.h"
#include "Style.h"

#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace {
constexpr int kBtn = 36, kPad = 6, kGap = 4, kSep = 9;
}

ToolBar::ToolBar(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    const QList<Item> defs = {
        {TRect, Icon::Rect, QStringLiteral("矩形  R"), {}, false},
        {TEllipse, Icon::Ellipse, QStringLiteral("椭圆  E"), {}, false},
        {TArrow, Icon::Arrow, QStringLiteral("箭头  A"), {}, false},
        {TPen, Icon::Pen, QStringLiteral("画笔  P"), {}, false},
        {TText, Icon::Text, QStringLiteral("文字  T"), {}, false},
        {TMosaic, Icon::Mosaic, QStringLiteral("马赛克  M"), {}, false},
        {TNumber, Icon::Number, QStringLiteral("序号  N"), {}, false},
        {-1, Icon::Rect, {}, {}, true},
        {Undo, Icon::Undo, QStringLiteral("撤销  Ctrl+Z"), {}, false},
        {Redo, Icon::Redo, QStringLiteral("重做  Ctrl+Y"), {}, false},
        {-1, Icon::Rect, {}, {}, true},
        {LongShot, Icon::Scroll, QStringLiteral("长截图（滚动）  L"), {}, false},
        {Pin, Icon::Pin, QStringLiteral("钉在桌面  F3"), {}, false},
        {Save, Icon::Save, QStringLiteral("快速保存  Ctrl+S（另存为 Ctrl+Shift+S）"), {}, false},
        {Cancel, Icon::Close, QStringLiteral("取消  Esc"), {}, false},
        {Confirm, Icon::Confirm, QStringLiteral("完成  Enter（默认动作见设置）"), {}, false},
    };
    int x = Margin + kPad;
    for (Item it : defs) {
        const int w = it.separator ? kSep : kBtn;
        it.rect = QRect(x, Margin + kPad, w, kBtn);
        x += w + kGap;
        m_items.append(it);
    }
    resize(x - kGap + kPad + Margin, kBtn + 2 * kPad + 2 * Margin);
}

void ToolBar::setActive(int id)
{
    m_active = id;
    update();
}

void ToolBar::setUndoRedo(bool u, bool r)
{
    m_canUndo = u;
    m_canRedo = r;
    update();
}

bool ToolBar::isEnabled(int id) const
{
    if (id == Undo) return m_canUndo;
    if (id == Redo) return m_canRedo;
    return true;
}

int ToolBar::itemAt(const QPoint& p) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (!m_items[i].separator && m_items[i].rect.contains(p)) return i;
    return -1;
}

void ToolBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF body = QRectF(rect()).adjusted(Margin, Margin, -Margin, -Margin);
    Style::paintGlass(p, body.adjusted(0.5, 0.5, -0.5, -0.5), 14, false);
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        if (it.separator) {
            p.setPen(QPen(QColor(0, 0, 0, 22), 1));
            const qreal cx = it.rect.center().x() + 0.5;
            p.drawLine(QPointF(cx, it.rect.top() + 9), QPointF(cx, it.rect.bottom() - 8));
            continue;
        }
        const bool enabled = isEnabled(it.id);
        const bool active = it.id == m_active;
        QColor fg = Style::Text;
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(Style::Accent);
            p.drawRoundedRect(QRectF(it.rect), 10, 10);
            fg = Qt::white;
        } else if (i == m_hover && enabled) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 13));
            p.drawRoundedRect(QRectF(it.rect), 10, 10);
        }
        if (it.id == Confirm) fg = active ? Qt::white : Style::Accent;
        if (!enabled) fg.setAlphaF(0.3f);
        const QRectF ir(it.rect.center().x() - 9.5 + 0.5, it.rect.center().y() - 9.5 + 0.5, 19, 19);
        paintIcon(p, it.icon, ir, fg);
    }
}

void ToolBar::mouseMoveEvent(QMouseEvent* e)
{
    const int h = itemAt(e->position().toPoint());
    if (h != m_hover) {
        m_hover = h;
        update();
        if (h >= 0) QToolTip::showText(e->globalPosition().toPoint(), m_items[h].tip, this, m_items[h].rect, 2500);
        else QToolTip::hideText();
    }
}

void ToolBar::mousePressEvent(QMouseEvent* e) { e->accept(); }

void ToolBar::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const int h = itemAt(e->position().toPoint());
    if (h >= 0 && isEnabled(m_items[h].id)) emit triggered(m_items[h].id);
}

void ToolBar::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

// ---------------- StyleBar ----------------

StyleBar::StyleBar(QWidget* parent) : QWidget(parent), m_palette(Style::annotationColors())
{
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    m_color = m_palette.first();
    relayout();
}

void StyleBar::configure(bool textMode, bool showColors, int sizeIndex, const QColor& color)
{
    m_text = textMode;
    m_colors = showColors;
    m_size = sizeIndex;
    m_color = color;
    relayout();
    update();
}

void StyleBar::relayout()
{
    m_sizeRects.clear();
    m_colorRects.clear();
    int x = Margin + 8;
    const int top = Margin + 6;
    for (int i = 0; i < 3; ++i) {
        m_sizeRects.append(QRect(x, top, 30, 30));
        x += 34;
    }
    if (m_colors) {
        x += 4;
        m_sepX = x;
        x += 9;
        for (int i = 0; i < m_palette.size(); ++i) {
            m_colorRects.append(QRect(x, top + 4, 22, 22));
            x += 28;
        }
        x -= 6;
    } else {
        x -= 4;
    }
    resize(x + 8 + Margin, 30 + 12 + 2 * Margin);
}

void StyleBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    Style::paintGlass(p, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, false);
    for (int i = 0; i < 3; ++i) {
        const QRectF r = m_sizeRects[i];
        if (i == m_size) {
            p.setPen(Qt::NoPen);
            p.setBrush(Style::AccentTint);
            p.drawRoundedRect(r, 8, 8);
        } else if (m_hover == i) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 13));
            p.drawRoundedRect(r, 8, 8);
        }
        const QColor fg = i == m_size ? Style::Accent : Style::Text2;
        if (m_text) {
            QFont f(Style::uiFontFamily());
            f.setPixelSize(11 + i * 3);
            f.setWeight(QFont::DemiBold);
            p.setFont(f);
            p.setPen(fg);
            p.drawText(r, Qt::AlignCenter, QStringLiteral("A"));
        } else {
            const qreal d = 4 + i * 3;
            p.setPen(Qt::NoPen);
            p.setBrush(fg);
            p.drawEllipse(r.center(), d / 2, d / 2);
        }
    }
    if (!m_colors) return;
    p.setPen(QPen(QColor(0, 0, 0, 22), 1));
    p.drawLine(QPointF(m_sepX + 4.5, Margin + 12), QPointF(m_sepX + 4.5, height() - Margin - 12));
    for (int i = 0; i < m_colorRects.size(); ++i) {
        const QRectF r = m_colorRects[i];
        const QColor c = m_palette[i];
        const bool sel = c.rgb() == m_color.rgb();
        if (sel) {
            p.setPen(QPen(Style::Accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r.adjusted(-2, -2, 2, 2));
        }
        p.setPen(c.lightness() > 230 ? QPen(QColor(0, 0, 0, 40), 1) : Qt::NoPen);
        p.setBrush(c);
        const qreal shrink = sel ? 3 : (m_hover == 100 + i ? 1 : 2);
        p.drawEllipse(r.adjusted(shrink, shrink, -shrink, -shrink));
    }
}

void StyleBar::mousePressEvent(QMouseEvent* e)
{
    const QPoint pt = e->position().toPoint();
    for (int i = 0; i < m_sizeRects.size(); ++i)
        if (m_sizeRects[i].contains(pt)) {
            m_size = i;
            update();
            emit sizeChanged(i);
            return;
        }
    for (int i = 0; i < m_colorRects.size(); ++i)
        if (m_colorRects[i].contains(pt)) {
            m_color = m_palette[i];
            update();
            emit colorChanged(m_color);
            return;
        }
}

void StyleBar::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint pt = e->position().toPoint();
    int h = -1;
    for (int i = 0; i < m_sizeRects.size(); ++i)
        if (m_sizeRects[i].contains(pt)) h = i;
    for (int i = 0; i < m_colorRects.size(); ++i)
        if (m_colorRects[i].contains(pt)) h = 100 + i;
    if (h != m_hover) {
        m_hover = h;
        update();
    }
}

void StyleBar::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}
