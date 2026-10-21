#include "Theme.h"
#include "RecordSession.h"
#include "HotkeyManager.h"
#include "Icons.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "Style.h"

#include <QDir>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QToolButton>
#include <windows.h>

namespace {
class Frame : public QWidget {
public:
    explicit Frame(const QRect& logical)
        : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowTransparentForInput)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setGeometry(logical.adjusted(-3, -3, 3, 3));
    }
    bool paused = false;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setPen(QPen(paused ? Style::Text3 : Style::Danger, 2, Qt::DashLine));
        p.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
};

QRect toLogical(const QRect& phys)
{
    for (QScreen* s : QGuiApplication::screens()) {
        const QRect mon = ScreenGrabber::monitorRect(s);
        if (mon.contains(phys.center())) {
            const qreal d = qreal(mon.width()) / s->geometry().width();
            return QRectF(s->geometry().topLeft() + QPointF(phys.topLeft() - mon.topLeft()) / d, QSizeF(phys.size()) / d)
                .toAlignedRect();
        }
    }
    return phys;
}

void exclude(QWidget* w) { SetWindowDisplayAffinity(reinterpret_cast<HWND>(w->winId()), 0x00000011 /* WDA_EXCLUDEFROMCAPTURE */); }

QIcon pauseIcon(bool paused)
{
    QPixmap pm(40, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Style::Text);
    if (paused) p.drawPolygon(QPolygonF{QPointF(14, 10), QPointF(30, 20), QPointF(14, 30)});
    else {
        p.drawRoundedRect(QRectF(12, 10, 5, 20), 1.5, 1.5);
        p.drawRoundedRect(QRectF(23, 10, 5, 20), 1.5, 1.5);
    }
    return QIcon(pm);
}

QIcon stopIcon()
{
    QPixmap pm(40, 40);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Style::Danger);
    p.drawRoundedRect(QRectF(11, 11, 18, 18), 4, 4);
    return QIcon(pm);
}
}

RecordSession::RecordSession(const QRect& region, Recorder::Format format)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    const QString ext = format == Recorder::Format::Mp4 ? QStringLiteral(".mp4") : QStringLiteral(".gif");
    const QString path = QDir(Settings::saveDir()).filePath(Settings::expandPattern(Settings::filePattern()) + ext);
    m_rec = new Recorder(region, format, Settings::recordFps(), Settings::recordCursor(), path);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(22, 18, 18, 18);
    lay->setSpacing(6);
    auto* dot = new QLabel(this);
    dot->setFixedSize(10, 10);
    Theme::setSheet(dot, QStringLiteral("background:#FF453A;border-radius:5px;"));
    lay->addWidget(dot);
    m_time = new QLabel(QStringLiteral("00:00"), this);
    Theme::setSheet(m_time, QStringLiteral("font-family:Consolas;font-size:14px;color:#1C1C1E;padding:0 6px;"));
    lay->addWidget(m_time);
    auto mk = [this](const QIcon& icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setIcon(icon);
        b->setIconSize(QSize(20, 20));
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        Theme::setSheet(b, QStringLiteral("QToolButton{border:none;border-radius:8px;padding:5px;}QToolButton:hover{background:rgba(0,0,0,13);}"));
        return b;
    };
    m_pause = mk(pauseIcon(false), QStringLiteral("暂停"));
    m_stop = mk(stopIcon(), QStringLiteral("停止并保存（Esc）"));
    lay->addWidget(m_pause);
    lay->addWidget(m_stop);
    connect(m_pause, &QToolButton::clicked, this, [this] {
        m_rec->setPaused(!m_rec->isPaused());
        m_pause->setIcon(pauseIcon(m_rec->isPaused()));
        m_pause->setToolTip(m_rec->isPaused() ? QStringLiteral("继续") : QStringLiteral("暂停"));
        static_cast<Frame*>(m_frame)->paused = m_rec->isPaused();
        m_frame->update();
    });
    connect(m_stop, &QToolButton::clicked, this, &RecordSession::finish);

    const QRect logical = toLogical(region);
    m_frame = new Frame(logical);
    adjustSize();
    QScreen* scr = QGuiApplication::screenAt(logical.center());
    const QRect avail = (scr ? scr : QGuiApplication::primaryScreen())->availableGeometry();
    QPoint pos(logical.center().x() - width() / 2, logical.bottom() + 6);
    if (pos.y() + height() > avail.bottom()) pos.setY(logical.top() - height() - 6);
    if (pos.y() < avail.top()) pos.setY(logical.bottom() - height() - 8);
    pos.setX(qBound(avail.left(), pos.x(), avail.right() - width()));
    move(pos);

    m_esc = new HotkeyManager(this);
    connect(m_esc, &HotkeyManager::activated, this, &RecordSession::finish);
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &RecordSession::refresh);
}

RecordSession::~RecordSession()
{
    delete m_rec;
    delete m_frame;
}

bool RecordSession::begin(QString* error)
{
    m_frame->show();
    show();
    exclude(m_frame);
    exclude(this);
    if (!m_rec->start(error)) return false;
    m_esc->setHotkey(QKeySequence(Qt::Key_Escape));
    m_tick.start();
    return true;
}

void RecordSession::refresh()
{
    const qint64 s = m_rec->elapsedMs() / 1000;
    m_time->setText(QStringLiteral("%1:%2").arg(s / 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0')));
}

void RecordSession::finish()
{
    if (m_done) return;
    m_done = true;
    m_tick.stop();
    m_esc->unregister();
    m_frame->hide();
    hide();
    m_rec->stop();
    const bool ok = m_rec->lastError().isEmpty() && m_rec->framesWritten() > 0;
    emit finished(ok, m_rec->outputPath(), ok ? QString() : (m_rec->lastError().isEmpty() ? QStringLiteral("没有录到画面") : m_rec->lastError()));
    close();
}

void RecordSession::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    Style::paintGlass(p, QRectF(rect()).adjusted(10, 10, -10, -10), 14);
}

void RecordSession::mousePressEvent(QMouseEvent* e) { m_drag = e->globalPosition().toPoint() - pos(); }

void RecordSession::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton) move(e->globalPosition().toPoint() - m_drag);
}
