#include <QCoreApplication>
#include "Theme.h"
#include "ScrollCapture.h"
#include "Logic.h"
#include "HotkeyManager.h"
#include "Icons.h"
#include "PinWindow.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "Style.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QScreen>
#include <QVBoxLayout>
#include <windows.h>

namespace {
// 选区外围的虚线框，鼠标穿透且不被截进图里
class RegionFrame : public QWidget {
public:
    explicit RegionFrame(const QRect& logical)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setGeometry(logical.adjusted(-3, -3, 3, 3));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        QPen pen(Style::Accent, 2, Qt::DashLine);
        p.setPen(pen);
        p.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
};

void excludeFromCapture(QWidget* w)
{
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(w->winId()), 0x00000011 /* WDA_EXCLUDEFROMCAPTURE */);
}

QRect physicalToLogical(const QRect& phys)
{
    for (QScreen* s : QGuiApplication::screens()) {
        const QRect mon = ScreenGrabber::monitorRect(s);
        if (mon.contains(phys.center())) {
            const qreal d = qreal(mon.width()) / s->geometry().width();
            const QPointF tl = s->geometry().topLeft() + QPointF(phys.topLeft() - mon.topLeft()) / d;
            return QRectF(tl, QSizeF(phys.size()) / d).toAlignedRect();
        }
    }
    return phys;
}

QPushButton* makeButton(const QString& text, QWidget* parent, bool primary = false)
{
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    if (primary) b->setObjectName(QStringLiteral("primary"));
    return b;
}
}

ScrollCapture::ScrollCapture(const QRect& region)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool), m_region(region)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(QCoreApplication::translate("QingYing", "长截图"));
    Theme::setSheet(this, QStringLiteral(R"(
        QLabel { color: #1C1C1E; font-size: 12px; }
        QLabel#status { color: #6E6E73; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 5px 10px; font-size: 12px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
    )"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(22, 22, 22, 22);
    root->setSpacing(8);
    m_status = new QLabel(QCoreApplication::translate("QingYing", "正在滚动截取…"), this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_status->setFixedWidth(180);
    root->addWidget(m_status);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(180, 320);
    m_preview->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    root->addWidget(m_preview);
    m_stop = makeButton(QCoreApplication::translate("QingYing", "停止（单击 / Esc）"), this, true);
    root->addWidget(m_stop);
    m_actions = new QWidget(this);
    auto* grid = new QVBoxLayout(m_actions);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    auto* copy = makeButton(QCoreApplication::translate("QingYing", "复制"), m_actions, true);
    auto* save = makeButton(QCoreApplication::translate("QingYing", "保存"), m_actions);
    auto* saveAs = makeButton(QCoreApplication::translate("QingYing", "另存为"), m_actions);
    auto* pin = makeButton(QCoreApplication::translate("QingYing", "钉图"), m_actions);
    auto* cancel = makeButton(QCoreApplication::translate("QingYing", "取消"), m_actions);
    auto* outputRow = new QHBoxLayout;
    auto* moreRow = new QHBoxLayout;
    for (auto* b : {copy, save, pin}) outputRow->addWidget(b);
    for (auto* b : {saveAs, cancel}) moreRow->addWidget(b);
    grid->addLayout(outputRow);
    grid->addLayout(moreRow);
    m_exports = {copy, save, saveAs, pin};
    copy->setToolTip(QStringLiteral("Ctrl+C / Enter"));
    save->setToolTip(QStringLiteral("Ctrl+S"));
    saveAs->setToolTip(QStringLiteral("Ctrl+Shift+S"));
    pin->setToolTip(QStringLiteral("F3"));
    m_actions->hide();
    root->addWidget(m_actions);
    connect(m_stop, &QPushButton::clicked, this, [this] { stop(QCoreApplication::translate("QingYing", "已停止")); });
    connect(copy, &QPushButton::clicked, this, [this] { finish(QStringLiteral("copy")); });
    connect(save, &QPushButton::clicked, this, [this] { finish(QStringLiteral("save")); });
    connect(saveAs, &QPushButton::clicked, this, [this] { finish(QStringLiteral("saveAs")); });
    connect(pin, &QPushButton::clicked, this, [this] { finish(QStringLiteral("pin")); });
    connect(cancel, &QPushButton::clicked, this, &QWidget::close);
    auto shortcut = [this](const QKeySequence& key, const QString& action) {
        auto* s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, [this, action] { if (!m_running) finish(action); });
    };
    shortcut(QKeySequence::Copy, QStringLiteral("copy"));
    shortcut(QKeySequence(Qt::Key_Return), QStringLiteral("copy"));
    shortcut(QKeySequence(Qt::Key_Enter), QStringLiteral("copy"));
    shortcut(QKeySequence::Save, QStringLiteral("save"));
    shortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), QStringLiteral("saveAs"));
    shortcut(QKeySequence(Qt::Key_F3), QStringLiteral("pin"));

    m_esc = new HotkeyManager(this);
    connect(m_esc, &HotkeyManager::activated, this, [this] {
        if (m_running) stop(QCoreApplication::translate("QingYing", "已停止"));
        else close();
    });

    const QRect logical = physicalToLogical(region);
    m_frame = new RegionFrame(logical);
    adjustSize();
    const QRect avail = QGuiApplication::screenAt(logical.center()) ? QGuiApplication::screenAt(logical.center())->availableGeometry()
                                                                      : QGuiApplication::primaryScreen()->availableGeometry();
    QPoint pos(logical.right() + 12, logical.top());
    if (pos.x() + width() > avail.right()) pos.setX(logical.left() - 12 - width());
    if (pos.x() < avail.left()) pos.setX(avail.right() - width());
    pos.setY(qBound(avail.top(), pos.y(), avail.bottom() - height()));
    move(pos);

    m_timer.setInterval(60);
    connect(&m_timer, &QTimer::timeout, this, &ScrollCapture::step);
}

ScrollCapture::~ScrollCapture()
{
    delete m_frame;
}

void ScrollCapture::start()
{
    m_frame->show();
    show();
    excludeFromCapture(m_frame);
    excludeFromCapture(this);
    m_esc->setHotkey(QKeySequence(Qt::Key_Escape));
    m_running = true;
    m_last = ScreenGrabber::grab(m_region);
    if (!m_buffer.reset(m_last)) {
        stop(QCoreApplication::translate("QingYing", "无法截取选区，请重新选择或缩小范围"));
        return;
    }
    updatePreview();
    // 等待鼠标按键松开后再开始，避免把触发截图的点击当作“停止”
    QTimer::singleShot(400, this, [this] { if (m_running) m_timer.start(); });
}

void ScrollCapture::step()
{
    if (!m_running) return;
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        stop(QCoreApplication::translate("QingYing", "已停止"));
        return;
    }
    if (!m_waitRender) {
        SetCursorPos(m_region.center().x(), m_region.center().y());
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = DWORD(-WHEEL_DELTA);
        SendInput(1, &in, sizeof(INPUT));
        m_waitRender = true;
        m_timer.setInterval(220);
        return;
    }
    m_waitRender = false;
    m_timer.setInterval(30);
    const QImage cur = ScreenGrabber::grab(m_region);
    const auto match = ScrollStitcher::match(m_last, cur);
    const int dy = match.shift;
    if (dy == 0) {
        if (++m_stillCount >= 3) stop(QCoreApplication::translate("QingYing", "已到底部"));
        return;
    }
    m_stillCount = 0;
    if (dy < 0) {
        // Wait for the same viewport to settle instead of scrolling past missing content.
        m_waitRender = true;
        m_timer.setInterval(220);
        if (++m_failCount >= 4) stop(QCoreApplication::translate("QingYing", "无法对齐画面，已停止（内容可能在变化）"));
        return;
    }
    m_failCount = 0;
    const auto added = m_buffer.append(cur, match);
    if (added != ScrollStitcher::Buffer::AppendResult::Added) {
        stop(added == ScrollStitcher::Buffer::AppendResult::LimitReached
                 ? QCoreApplication::translate("QingYing", "已达到长图大小上限，已保留当前结果")
                 : QCoreApplication::translate("QingYing", "无法继续拼接，已保留当前结果"));
        return;
    }
    m_last = cur;
    updatePreview();
}

int ScrollCapture::findShift(const QImage& a, const QImage& b) { return Logic::findShift(a, b); }

void ScrollCapture::stop(const QString& reason)
{
    if (!m_running) return;
    m_running = false;
    m_timer.stop();
    m_esc->unregister(); // Let save dialogs handle Escape themselves.
    m_frame->hide();
    m_status->setText(QStringLiteral("%1 · %2×%3").arg(reason).arg(m_buffer.size().width()).arg(m_buffer.size().height()));
    for (auto* b : m_exports) b->setEnabled(!m_buffer.isEmpty());
    m_stop->hide();
    m_actions->show();
    adjustSize();
    raise();
    activateWindow();
}

void ScrollCapture::finish(const QString& action)
{
    if (m_running || m_buffer.isEmpty()) return;
    if (m_result.isNull()) m_result = m_buffer.image();
    if (m_result.isNull()) {
        m_status->setText(QCoreApplication::translate("QingYing", "内存不足，请关闭其他程序后重试；当前长图仍保留"));
        adjustSize();
        return;
    }
    QString saved;
    if (action == QLatin1String("copy")) {
        QGuiApplication::clipboard()->setImage(m_result);
    } else if (action == QLatin1String("save") || action == QLatin1String("saveAs")) {
        saved = Settings::defaultFilePath();
        if (action == QLatin1String("saveAs")) {
            const QString png = QCoreApplication::translate("QingYing", "PNG 图片 (*.png)");
            const QString jpg = QCoreApplication::translate("QingYing", "JPG 图片 (*.jpg *.jpeg)");
            QString filter = Settings::format() == QLatin1String("jpg") ? jpg : png;
            QDir().mkpath(Settings::saveDir());
            saved = QFileDialog::getSaveFileName(this, QCoreApplication::translate("QingYing", "另存为"),
                                               saved, png + QStringLiteral(";;") + jpg, &filter);
            if (saved.isEmpty()) return;
        }
        if (!Settings::saveImage(m_result, saved)) {
            m_status->setText(QCoreApplication::translate("QingYing", "保存失败，长图已保留。请重试或使用“另存为”更换位置。"));
            adjustSize();
            return;
        }
    } else if (action == QLatin1String("pin")) {
        QImage img = m_result;
        const QRect logical = physicalToLogical(m_region);
        img.setDevicePixelRatio(qreal(m_region.width()) / qMax(1, logical.width()));
        auto* pin = new PinWindow(img);
        pin->placeImageAt(logical.topLeft());
        pin->show();
    } else return;
    emit delivered(m_result, saved);
    close();
}

void ScrollCapture::updatePreview()
{
    m_preview->setPixmap(QPixmap::fromImage(m_buffer.preview(m_preview->size())));
    m_status->setText(QCoreApplication::translate("QingYing", "正在滚动截取 · %1 帧 · %2×%3")
                          .arg(m_buffer.frameCount()).arg(m_buffer.size().width()).arg(m_buffer.size().height()));
    adjustSize();
}

void ScrollCapture::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    Style::paintGlass(p, QRectF(rect()).adjusted(12, 12, -12, -12), 14);
}

void ScrollCapture::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        if (m_running) stop(QCoreApplication::translate("QingYing", "已停止"));
        else close();
    }
}
