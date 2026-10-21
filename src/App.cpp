#include "App.h"
#include "CaptureOverlay.h"
#include "History.h"
#include "HistoryWindow.h"
#include "Icons.h"
#include "Ocr.h"
#include "QrDecoder.h"
#include "PinWindow.h"
#include "ScrollCapture.h"
#include "RecordSession.h"
#include "Recorder.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QImageReader>
#include <QUrl>
#include <QProcess>
#include <chrono>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QPainter>
#include <thread>

App::App()
{
    m_menu = new QMenu;
    m_menu->setStyleSheet(QStringLiteral(
        "QMenu{background:#FFFFFF;border:1px solid rgba(0,0,0,20);border-radius:10px;padding:6px;}"
        "QMenu::item{padding:7px 28px 7px 12px;border-radius:6px;color:#1C1C1E;}"
        "QMenu::item:selected{background:rgba(79,107,255,31);color:#1C1C1E;}"
        "QMenu::separator{height:1px;background:rgba(0,0,0,15);margin:5px 8px;}"
        "QMenu::icon{padding-left:8px;}"));
    m_captureAct = m_menu->addAction(menuIcon(Icon::Capture), QStringLiteral("截图"));
    connect(m_captureAct, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, [this] { startCapture(); }); });
    QMenu* delay = m_menu->addMenu(menuIcon(Icon::Clock), QStringLiteral("延时截图"));
    for (int s : {3, 5, 10})
        delay->addAction(QStringLiteral("%1 秒后").arg(s), this, [this, s] { startDelayedCapture(s); });
    m_menu->addAction(menuIcon(Icon::Scroll), QStringLiteral("长截图（滚动）"), this,
                      [this] { QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::LongShot)); }); });
    m_menu->addAction(menuIcon(Icon::Picker), QStringLiteral("取色"), this,
                      [this] { QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Pick)); }); });
    QMenu* rec = m_menu->addMenu(menuIcon(Icon::Record), QStringLiteral("录屏"));
    rec->addAction(QStringLiteral("录制 MP4"), this, [this] { m_recordGif = false; QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Record)); }); });
    rec->addAction(QStringLiteral("录制 GIF"), this, [this] { m_recordGif = true; QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Record)); }); });
    m_menu->addSeparator();
    m_pinMenu = m_menu->addMenu(menuIcon(Icon::Pin), QStringLiteral("钉图"));
    m_pinMenu->addAction(QStringLiteral("全部显示"), this, [] { PinWindow::showAll(); });
    m_pinMenu->addAction(QStringLiteral("全部隐藏"), this, [] { PinWindow::hideAll(); });
    m_pinMenu->addAction(QStringLiteral("全部关闭"), this, [] { PinWindow::closeAll(); });
    m_recentMenu = m_menu->addMenu(menuIcon(Icon::History), QStringLiteral("最近截图"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &App::rebuildRecentMenu);
    m_menu->addSeparator();
    m_menu->addAction(menuIcon(Icon::Settings), QStringLiteral("设置…"), this, &App::openSettings);
    m_autoAct = m_menu->addAction(QStringLiteral("开机自启"));
    m_autoAct->setCheckable(true);
    connect(m_autoAct, &QAction::toggled, this, [](bool on) { Settings::setAutostart(on); });
    m_menu->addAction(QStringLiteral("关于青影"), this, &App::showAbout);
    m_menu->addSeparator();
    m_menu->addAction(QStringLiteral("退出"), qApp, &QApplication::quit);
    connect(m_menu, &QMenu::aboutToShow, this, &App::updateMenu);

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) QTimer::singleShot(120, this, [this] { startCapture(); });
    });
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        if (m_lastRecording.isEmpty()) return;
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(m_lastRecording)});
        m_lastRecording.clear();
    });
    connect(&m_hotkey, &HotkeyManager::activated, this, [this] { startCapture(); });
    const QKeySequence hk = Settings::hotkey();
    const bool ok = m_hotkey.setHotkey(hk);
    updateMenu();
    const QString hkText = hk.toString(QKeySequence::NativeText);
    if (ok)
        m_tray->showMessage(QStringLiteral("青影已在后台运行"), QStringLiteral("按 %1 开始截图").arg(hkText),
                            appIcon(), 3000);
    else
        m_tray->showMessage(QStringLiteral("快捷键冲突"),
                            QStringLiteral("%1 已被其他程序占用，请在设置中更换。也可单击托盘图标截图。").arg(hkText),
                            QSystemTrayIcon::Warning, 6000);
    writeSmokeReport();
}

App::~App()
{
    endCapture();
    delete m_menu;
}

void App::writeSmokeReport()
{
    const QString path = qEnvironmentVariable("QINGYING_SMOKE_REPORT");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
        f.write(QStringLiteral("tray_available=%1\ntray_visible=%2\nhotkey=%3\nhotkey_registered=%4\nscreens=%5\n")
                    .arg(QSystemTrayIcon::isSystemTrayAvailable())
                    .arg(m_tray->isVisible())
                    .arg(m_hotkey.current().toString())
                    .arg(m_hotkey.isRegistered())
                    .arg(QGuiApplication::screens().size())
                    .toUtf8());
    // 长截图拼接自检：构造一张随机纹理，平移 37 像素后应能找回位移
    QImage tall(300, 900, QImage::Format_RGB32);
    for (int y = 0; y < tall.height(); ++y)
        for (int x = 0; x < tall.width(); ++x) tall.setPixel(x, y, qRgb((x * 7 + y * 13) % 251, (y * y) % 253, (x ^ y) & 255));
    const QImage a = tall.copy(0, 0, 300, 400), b = tall.copy(0, 37, 300, 400);
    f.write(QStringLiteral("stitch_shift=%1 (expect 37)\nstitch_same=%2 (expect 0)\n")
                .arg(ScrollCapture::findShift(a, b)).arg(ScrollCapture::findShift(a, a)).toUtf8());
    // OCR 自检：渲染一段文字后识别
    QImage txt(520, 90, QImage::Format_RGB32);
    txt.fill(Qt::white);
    {
        QPainter tp(&txt);
        QFont tf(QStringLiteral("Microsoft YaHei UI"));
        tf.setPixelSize(36);
        tp.setFont(tf);
        tp.setPen(Qt::black);
        tp.drawText(txt.rect(), Qt::AlignCenter, QStringLiteral("Hello 青影 2026"));
    }
    Ocr::Result ocr;
    std::thread([&] { ocr = Ocr::recognize(txt); }).join();
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_RECORD")) {
        for (auto fmt : {Recorder::Format::Mp4, Recorder::Format::Gif}) {
            const QString out = QDir::temp().filePath(fmt == Recorder::Format::Mp4 ? QStringLiteral("qy_smoke.mp4") : QStringLiteral("qy_smoke.gif"));
            QFile::remove(out);
            Recorder rec(QRect(0, 0, 320, 240), fmt, 15, true, out);
            QString err;
            const bool started = rec.start(&err);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            rec.stop();
            f.write(QStringLiteral("record_%1 started=%2 frames=%3 bytes=%4 error=%5\n")
                        .arg(fmt == Recorder::Format::Mp4 ? QStringLiteral("mp4") : QStringLiteral("gif"))
                        .arg(started).arg(rec.framesWritten()).arg(QFileInfo(out).size()).arg(err + rec.lastError()).toUtf8());
        }
    }
    const QString qrPath = qEnvironmentVariable("QINGYING_SMOKE_QR");
    if (!qrPath.isEmpty()) {
        const auto codes = QrDecoder::decode(QImage(qrPath));
        f.write(QStringLiteral("qr_count=%1\nqr_text=%2\n").arg(codes.size())
                    .arg(codes.isEmpty() ? QString() : codes.first().format + QStringLiteral(": ") + codes.first().text).toUtf8());
    }
    f.write(QStringLiteral("ocr_ok=%1\nocr_lang=%2\nocr_text=%3\nocr_error=%4\n")
                .arg(ocr.ok).arg(ocr.language, ocr.text, ocr.error).toUtf8());
}

void App::updateMenu()
{
    m_captureAct->setText(QStringLiteral("截图\t%1").arg(Settings::hotkey().toString(QKeySequence::NativeText)));
    const int pins = PinWindow::all().size();
    m_pinMenu->setTitle(pins ? QStringLiteral("钉图（%1）").arg(pins) : QStringLiteral("钉图"));
    m_pinMenu->setEnabled(pins > 0);
    const QSignalBlocker b(m_autoAct);
    m_autoAct->setChecked(Settings::autostart());
    m_tray->setToolTip(QStringLiteral("青影 截图 · %1").arg(Settings::hotkey().toString(QKeySequence::NativeText)));
}

bool App::applyHotkey(const QKeySequence& seq)
{
    const QKeySequence old = m_hotkey.current();
    if (m_hotkey.isRegistered() && old == seq) return true;
    if (m_hotkey.setHotkey(seq)) {
        updateMenu();
        return true;
    }
    if (!old.isEmpty()) m_hotkey.setHotkey(old);
    return false;
}

void App::startDelayedCapture(int seconds)
{
    m_tray->showMessage(QStringLiteral("延时截图"), QStringLiteral("%1 秒后开始截图").arg(seconds), appIcon(), 1500);
    QTimer::singleShot(seconds * 1000, this, [this] { startCapture(); });
}

void App::startLongShot(const QRect& region)
{
    if (m_busy) return;
    m_busy = true;
    auto* sc = new ScrollCapture(region);
    connect(sc, &ScrollCapture::delivered, this, &App::onDelivered);
    connect(sc, &QObject::destroyed, this, [this] { m_busy = false; });
    sc->start();
}

void App::startRecording(const QRect& region)
{
    if (m_busy) return;
    m_busy = true;
    auto* s = new RecordSession(region, m_recordGif ? Recorder::Format::Gif : Recorder::Format::Mp4);
    connect(s, &QObject::destroyed, this, [this] { m_busy = false; });
    connect(s, &RecordSession::finished, this, [this](bool ok, const QString& path, const QString& err) {
        if (ok) {
            m_lastRecording = path;
            m_tray->showMessage(QStringLiteral("录制完成"), QStringLiteral("%1\n点击打开所在文件夹").arg(QDir::toNativeSeparators(path)),
                                appIcon(), 4000);
        } else {
            m_tray->showMessage(QStringLiteral("录制失败"), err, QSystemTrayIcon::Warning, 5000);
        }
    });
    QString err;
    if (!s->begin(&err)) {
        m_tray->showMessage(QStringLiteral("无法开始录制"), err, QSystemTrayIcon::Warning, 5000);
        s->close();
    }
}

void App::onDelivered(const QImage& image, const QString& path)
{
    History::add(image);
    if (!path.isEmpty())
        m_tray->showMessage(QStringLiteral("截图已保存"), QDir::toNativeSeparators(path), appIcon(), 2500);
    if (m_history) m_history->reload();
}

void App::rebuildRecentMenu()
{
    m_recentMenu->clear();
    const QStringList list = History::entries();
    for (int i = 0; i < qMin<qsizetype>(8, list.size()); ++i) {
        const QString path = list[i];
        QImageReader r(path);
        const QSize full = r.size();
        r.setScaledSize(full.scaled(QSize(48, 32), Qt::KeepAspectRatio));
        const QString stamp = QFileInfo(path).baseName();
        auto* act = m_recentMenu->addAction(QIcon(QPixmap::fromImage(r.read())),
                                            QStringLiteral("%1:%2:%3  %4×%5  复制")
                                                .arg(stamp.mid(9, 2), stamp.mid(11, 2), stamp.mid(13, 2))
                                                .arg(full.width()).arg(full.height()));
        connect(act, &QAction::triggered, this, [path] { QGuiApplication::clipboard()->setImage(QImage(path)); });
    }
    if (list.isEmpty()) m_recentMenu->addAction(QStringLiteral("暂无记录"))->setEnabled(false);
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(QStringLiteral("历史记录…"), this, &App::openHistory);
    m_recentMenu->addAction(QStringLiteral("打开保存文件夹"), this, [] {
        QDir().mkpath(Settings::saveDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::saveDir()));
    });
}

void App::openHistory()
{
    if (!m_history) {
        m_history = new HistoryWindow;
        m_history->setAttribute(Qt::WA_DeleteOnClose);
    } else {
        m_history->reload();
    }
    m_history->show();
    m_history->raise();
    m_history->activateWindow();
}

void App::startCapture(int mode)
{
    if (!m_overlays.isEmpty() || m_busy) return;
    if (m_settings && m_settings->isVisible()) return;
    m_detector.snapshot();

    // 先冻结所有显示器，再显示遮罩
    struct Shot {
        QScreen* screen;
        QRect monitor;
        QImage image;
    };
    QList<Shot> shots;
    for (QScreen* s : QGuiApplication::screens()) {
        const QRect mon = ScreenGrabber::monitorRect(s);
        QImage img = ScreenGrabber::grab(mon);
        if (img.isNull()) continue;
        img.setDevicePixelRatio(qreal(mon.width()) / s->geometry().width());
        shots.append({s, mon, img});
    }
    CaptureOverlay* under = nullptr;
    for (const Shot& sh : shots) {
        auto* o = new CaptureOverlay(sh.screen, sh.image, sh.monitor, &m_detector);
        connect(o, &CaptureOverlay::finished, this, &App::endCapture, Qt::QueuedConnection);
        o->setMode(CaptureMode(mode));
        connect(o, &CaptureOverlay::colorPicked, this, [this](const QString& t) {
            m_tray->showMessage(QStringLiteral("已复制颜色"), t, appIcon(), 1500);
        });
        connect(o, &CaptureOverlay::delivered, this, &App::onDelivered);
        connect(o, &CaptureOverlay::recordRequested, this, [this](const QRect& r) {
            QTimer::singleShot(250, this, [this, r] { startRecording(r); });
        });
        connect(o, &CaptureOverlay::longShotRequested, this, [this](const QRect& r) {
            QTimer::singleShot(250, this, [this, r] { startLongShot(r); });
        });
        connect(o, &CaptureOverlay::selectionStarted, this, [this](CaptureOverlay* self) {
            for (auto& other : m_overlays)
                if (other && other != self) other->setPassive(true);
        });
        connect(o, &CaptureOverlay::selectionCleared, this, [this](CaptureOverlay* self) {
            for (auto& other : m_overlays)
                if (other && other != self) other->setPassive(false);
        });
        m_overlays.append(o);
        o->show();
        if (sh.screen->geometry().contains(QCursor::pos())) under = o;
    }
    if (!under && !m_overlays.isEmpty()) under = m_overlays.first();
    if (under) under->activate();
}

void App::endCapture()
{
    const auto list = m_overlays;
    m_overlays.clear();
    for (const auto& o : list)
        if (o) {
            o->hide();
            o->deleteLater();
        }
}

void App::openSettings()
{
    if (m_settings) {
        m_settings->raise();
        m_settings->activateWindow();
        return;
    }
    m_settings = new SettingsDialog([this](const QKeySequence& k) { return applyHotkey(k); });
    m_settings->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_settings, &QDialog::finished, this, [this] {
        updateMenu();
        History::prune();
    });
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void App::showAbout()
{
    QMessageBox box;
    box.setWindowTitle(QStringLiteral("关于青影"));
    box.setIconPixmap(appIcon().pixmap(64, 64));
    box.setText(QStringLiteral("<h3>青影 QingYing %1</h3>").arg(QApplication::applicationVersion()));
    box.setInformativeText(QStringLiteral("简洁好看的 Windows 截图工具。<br>区域截图 · 标注 · 钉图<br><br>"
                                          "基于 Qt %1 · MIT 许可证").arg(QString::fromLatin1(qVersion())));
    box.exec();
}
