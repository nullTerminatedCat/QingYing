#include <QCoreApplication>
#include "Theme.h"
#include "ScrollCapture.h"
#include "HotkeyManager.h"
#include "Icons.h"
#include "PinWindow.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "Style.h"

#include <QClipboard>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <windows.h>

namespace {
constexpr int kMaxHeight = 30000;

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
    setWindowTitle(QCoreApplication::translate("QingYing", "青影 长截图"));
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
    root->addWidget(m_status);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(180, 320);
    m_preview->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    root->addWidget(m_preview);
    m_stop = makeButton(QCoreApplication::translate("QingYing", "停止（单击 / Esc）"), this, true);
    root->addWidget(m_stop);
    m_actions = new QWidget(this);
    auto* grid = new QHBoxLayout(m_actions);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    auto* copy = makeButton(QCoreApplication::translate("QingYing", "复制"), m_actions, true);
    auto* save = makeButton(QCoreApplication::translate("QingYing", "保存"), m_actions);
    auto* pin = makeButton(QCoreApplication::translate("QingYing", "钉图"), m_actions);
    auto* cancel = makeButton(QCoreApplication::translate("QingYing", "取消"), m_actions);
    for (auto* b : {copy, save, pin, cancel}) grid->addWidget(b);
    m_actions->hide();
    root->addWidget(m_actions);
    connect(m_stop, &QPushButton::clicked, this, [this] { stop(QCoreApplication::translate("QingYing", "已停止")); });
    connect(copy, &QPushButton::clicked, this, [this] { finish(QStringLiteral("copy")); });
    connect(save, &QPushButton::clicked, this, [this] { finish(QStringLiteral("save")); });
    connect(pin, &QPushButton::clicked, this, [this] { finish(QStringLiteral("pin")); });
    connect(cancel, &QPushButton::clicked, this, &QWidget::close);

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
    m_result = ScreenGrabber::grab(m_region);
    m_last = m_result;
    updatePreview();
    m_running = true;
    // 等待鼠标按键松开后再开始，避免把触发截图的点击当作“停止”
    QTimer::singleShot(400, this, [this] { m_timer.start(); });
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
    const int dy = findShift(m_last, cur);
    if (dy == 0) {
        if (++m_stillCount >= 3) stop(QCoreApplication::translate("QingYing", "已到底部"));
        return;
    }
    m_stillCount = 0;
    if (dy < 0) {
        if (++m_failCount >= 4) stop(QCoreApplication::translate("QingYing", "无法对齐画面，已停止（内容可能在变化）"));
        return;
    }
    m_failCount = 0;
    const int h = cur.height();
    QImage merged(m_result.width(), m_result.height() + dy, QImage::Format_RGB32);
    {
        QPainter p(&merged);
        p.drawImage(0, 0, m_result);
        p.drawImage(QPoint(0, m_result.height()), cur, QRect(0, h - dy, cur.width(), dy));
    }
    m_result = merged;
    m_last = cur;
    updatePreview();
    if (m_result.height() >= kMaxHeight) stop(QCoreApplication::translate("QingYing", "已达到最大高度"));
}

int ScrollCapture::findShift(const QImage& a, const QImage& b)
{
    if (a.size() != b.size() || a.height() < 20) return -1;
    const QImage prev = a.convertToFormat(QImage::Format_RGB32);
    const QImage cur = b.convertToFormat(QImage::Format_RGB32);
    const int w = prev.width(), h = prev.height();
    const int x0 = w > 120 ? 8 : 0;
    const int x1 = w > 120 ? w - 24 : w; // 忽略滚动条
    auto rowInfo = [&](const QImage& img, QList<quint64>& hashes, QList<bool>& uniform) {
        hashes.resize(h);
        uniform.resize(h);
        for (int y = 0; y < h; ++y) {
            const QRgb* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            quint64 hsh = 1469598103934665603ULL;
            bool uni = true;
            for (int x = x0; x < x1; ++x) {
                hsh = (hsh ^ (row[x] & 0xFFFFFF)) * 1099511628211ULL;
                if (uni && row[x] != row[x0]) uni = false;
            }
            hashes[y] = hsh;
            uniform[y] = uni;
        }
    };
    QList<quint64> hp, hc;
    QList<bool> up, uc;
    rowInfo(prev, hp, up);
    rowInfo(cur, hc, uc);
    int best = -1;
    double bestRatio = 0;
    for (int dy = 0; dy < h - h / 5; ++dy) {
        int total = 0, match = 0;
        for (int y = 0; y + dy < h; ++y) {
            if (up[y + dy] && uc[y]) continue;
            ++total;
            if (hp[y + dy] == hc[y]) ++match;
        }
        if (total < 8) continue;
        const double ratio = double(match) / total;
        if (ratio > bestRatio + 1e-9) {
            bestRatio = ratio;
            best = dy;
        }
    }
    return bestRatio >= 0.85 ? best : -1;
}

void ScrollCapture::stop(const QString& reason)
{
    if (!m_running) return;
    m_running = false;
    m_timer.stop();
    m_status->setText(QStringLiteral("%1 · %2×%3").arg(reason).arg(m_result.width()).arg(m_result.height()));
    m_stop->hide();
    m_actions->show();
    adjustSize();
    raise();
    activateWindow();
}

void ScrollCapture::finish(const QString& action)
{
    QString saved;
    if (action == QLatin1String("copy")) {
        QGuiApplication::clipboard()->setImage(m_result);
    } else if (action == QLatin1String("save")) {
        saved = Settings::defaultFilePath();
        if (!Settings::saveImage(m_result, saved)) saved.clear();
    } else if (action == QLatin1String("pin")) {
        QImage img = m_result;
        const QRect logical = physicalToLogical(m_region);
        img.setDevicePixelRatio(qreal(m_region.width()) / qMax(1, logical.width()));
        auto* pin = new PinWindow(img);
        pin->placeImageAt(logical.topLeft());
        pin->show();
    }
    emit delivered(m_result, saved);
    close();
}

void ScrollCapture::updatePreview()
{
    const QImage scaled = m_result.scaledToWidth(m_preview->width(), Qt::SmoothTransformation);
    // 只显示底部，最新内容可见
    const int h = qMin(scaled.height(), m_preview->height());
    m_preview->setPixmap(QPixmap::fromImage(scaled.copy(0, scaled.height() - h, scaled.width(), h)));
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
