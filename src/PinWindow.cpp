#include "PinWindow.h"
#include "Icons.h"
#include "Recognize.h"
#include "Settings.h"
#include "Style.h"

#include <QClipboard>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTransform>

namespace {
QList<PinWindow*>& registry()
{
    static QList<PinWindow*> list;
    return list;
}
}

PinWindow::PinWindow(const QImage& image)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_img(image)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
    setWindowTitle(QStringLiteral("青影 钉图"));
    m_base = QSizeF(m_img.width() / m_img.devicePixelRatio(), m_img.height() / m_img.devicePixelRatio());
    updateSize();
    registry().append(this);
}

PinWindow::~PinWindow() { registry().removeAll(this); }

QList<PinWindow*> PinWindow::all() { return registry(); }

void PinWindow::hideAll()
{
    for (PinWindow* w : registry()) w->hide();
}

void PinWindow::showAll()
{
    for (PinWindow* w : registry()) w->show();
}

void PinWindow::closeAll()
{
    const auto list = registry();
    for (PinWindow* w : list) w->close();
}

void PinWindow::rotate(int degrees)
{
    const qreal dpr = m_img.devicePixelRatio();
    const QPoint c = geometry().center();
    m_img = m_img.transformed(QTransform().rotate(degrees));
    m_img.setDevicePixelRatio(dpr);
    if (qAbs(degrees) % 180 == 90) m_base = m_base.transposed();
    updateSize();
    move(c - rect().center());
}

void PinWindow::flip()
{
    const qreal dpr = m_img.devicePixelRatio();
    m_img = m_img.flipped(Qt::Horizontal);
    m_img.setDevicePixelRatio(dpr);
    update();
}

void PinWindow::placeImageAt(const QPoint& g) { move(g - QPoint(M, M)); }

QRectF PinWindow::imageRect() const { return QRectF(M, M, width() - 2 * M, height() - 2 * M); }

QRectF PinWindow::buttonRect(int i) const
{
    const QRectF ir = imageRect();
    const qreal s = 26;
    return QRectF(ir.right() - 8 - (3 - i) * (s + 2), ir.top() + 8, s, s);
}

int PinWindow::buttonAt(const QPointF& p) const
{
    if (!m_hover || imageRect().width() < 110 || imageRect().height() < 44) return -1;
    for (int i = 0; i < 3; ++i)
        if (buttonRect(i).contains(p)) return i;
    return -1;
}

void PinWindow::updateSize()
{
    const QSize s = (m_base * m_scale).toSize().expandedTo(QSize(8, 8));
    resize(s + QSize(2 * M, 2 * M));
    update();
}

void PinWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF ir = imageRect();
    Style::paintShadow(p, ir, 10, M - 2);
    QPainterPath clip;
    clip.addRoundedRect(ir, 10, 10);
    p.save();
    p.setClipPath(clip);
    p.drawImage(ir, m_img);
    p.restore();
    p.setPen(QPen(m_hover ? Style::Accent : QColor(79, 107, 255, 150), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(ir.adjusted(0.75, 0.75, -0.75, -0.75), 10, 10);

    if (!m_hover) return;
    if (ir.width() >= 110 && ir.height() >= 44) {
        const QRectF bar = buttonRect(0).united(buttonRect(2)).adjusted(-3, -3, 3, 3);
        p.setPen(Qt::NoPen);
        p.setBrush(Style::Glass);
        p.drawRoundedRect(bar, 9, 9);
        const Icon icons[3] = {Icon::Copy, Icon::Save, Icon::Close};
        for (int i = 0; i < 3; ++i) {
            const QRectF r = buttonRect(i);
            if (i == m_hoverBtn) {
                p.setBrush(QColor(0, 0, 0, 15));
                p.drawRoundedRect(r, 7, 7);
            }
            paintIcon(p, icons[i], r.adjusted(5, 5, -5, -5), i == 2 && i == m_hoverBtn ? Style::Danger : Style::Text);
        }
    }
    if (ir.height() >= 44) Style::paintPill(p, QPointF(ir.left() + 8, ir.bottom() - 8 - Style::pillSize(QStringLiteral("100%")).height()),
                                            QStringLiteral("%1%").arg(qRound(m_scale * 100)));
}

void PinWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    const int b = buttonAt(e->position());
    if (b == 0) return copy();
    if (b == 1) return save();
    if (b == 2) {
        close();
        return;
    }
    m_drag = true;
    m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
}

void PinWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (m_drag && (e->buttons() & Qt::LeftButton)) {
        move(e->globalPosition().toPoint() - m_dragOffset);
        return;
    }
    const int b = buttonAt(e->position());
    if (b != m_hoverBtn) {
        m_hoverBtn = b;
        update();
    }
}

void PinWindow::mouseReleaseEvent(QMouseEvent*) { m_drag = false; }

void PinWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && buttonAt(e->position()) < 0) {
        m_scale = 1;
        updateSize();
    }
}

void PinWindow::wheelEvent(QWheelEvent* e)
{
    const int steps = e->angleDelta().y() / 120;
    if (!steps) return;
    if (e->modifiers() & Qt::ControlModifier) {
        m_opacity = qBound(0.2, m_opacity + 0.1 * steps, 1.0);
        setWindowOpacity(m_opacity);
    } else {
        const QPointF before = e->position() - QPointF(M, M);
        const qreal old = m_scale;
        m_scale = qBound(0.1, m_scale * (steps > 0 ? 1.1 : 1 / 1.1), 8.0);
        const QPointF after = before * (m_scale / old);
        move(pos() + (before - after).toPoint());
        updateSize();
    }
}

void PinWindow::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) close();
    else if (e->matches(QKeySequence::Copy)) copy();
    else if (e->matches(QKeySequence::Save)) save();
    else if (e->key() == Qt::Key_R) rotate((e->modifiers() & Qt::ShiftModifier) ? -90 : 90);
    else if (e->key() == Qt::Key_H) flip();
}

void PinWindow::enterEvent(QEnterEvent*)
{
    m_hover = true;
    update();
}

void PinWindow::leaveEvent(QEvent*)
{
    m_hover = false;
    m_hoverBtn = -1;
    update();
}

void PinWindow::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu menu(this);
    menu.addAction(menuIcon(Icon::Copy), QStringLiteral("复制"), this, &PinWindow::copy);
    menu.addAction(menuIcon(Icon::Save), QStringLiteral("另存为…"), this, &PinWindow::save);
    menu.addAction(menuIcon(Icon::Ocr), QStringLiteral("识别文字（OCR）"), this, [this] { Recognize::ocr(m_img); });
    menu.addAction(menuIcon(Icon::Qr), QStringLiteral("识别二维码 / 条码"), this, [this] { Recognize::qr(m_img); });
    menu.addSeparator();
    menu.addAction(menuIcon(Icon::Rotate), QStringLiteral("顺时针旋转 90°\tR"), this, [this] { rotate(90); });
    menu.addAction(QStringLiteral("逆时针旋转 90°\tShift+R"), this, [this] { rotate(-90); });
    menu.addAction(QStringLiteral("水平翻转\tH"), this, &PinWindow::flip);
    menu.addAction(QStringLiteral("原始大小"), this, [this] {
        m_scale = 1;
        updateSize();
    });
    menu.addSeparator();
    menu.addAction(menuIcon(Icon::Close), QStringLiteral("关闭"), this, &QWidget::close);
    menu.exec(e->globalPos());
}

void PinWindow::copy()
{
    QImage img = m_img;
    img.setDevicePixelRatio(1);
    QGuiApplication::clipboard()->setImage(img);
}

void PinWindow::save()
{
    const QString png = QStringLiteral("PNG 图片 (*.png)"), jpg = QStringLiteral("JPG 图片 (*.jpg *.jpeg)");
    QString filter = Settings::format() == QLatin1String("jpg") ? jpg : png;
    QDir().mkpath(Settings::saveDir());
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存钉图"), Settings::defaultFilePath(),
                                                      png + QStringLiteral(";;") + jpg, &filter);
    if (!path.isEmpty() && !Settings::saveImage(m_img, path))
        QMessageBox::warning(this, QStringLiteral("青影"), QStringLiteral("保存失败：%1").arg(path));
}
