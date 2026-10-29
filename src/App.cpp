#include <QCoreApplication>
#include "Theme.h"
#include "App.h"
#include "Language.h"
#include <windows.h>
#include <dwmapi.h>
#include "Recognize.h"
#include "Diagnostics.h"
#include "EditRequest.h"
#include "SettingsRequest.h"
#include "CaptureOverlay.h"
#include "History.h"
#include "HistoryWindow.h"
#include "Icons.h"
#include "Notify.h"
#include "Updater.h"
#include "Secret.h"
#include "Uploader.h"
#include "Ocr.h"
#include "QrDecoder.h"
#include "PinWindow.h"
#include "ScrollCapture.h"
#include "RecordSession.h"
#include "Recorder.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include <QPointer>

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
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslSocket>
#include <QPainter>
#include <thread>

namespace {
QPointer<QSystemTrayIcon> g_tray;
}

namespace Notify {
void info(const QString& title, const QString& text)
{
    if (g_tray) g_tray->showMessage(title, text, appIcon(), 3000);
}
void warn(const QString& title, const QString& text)
{
    if (g_tray) g_tray->showMessage(title, text, QSystemTrayIcon::Warning, 5000);
}
}

App::App()
{
    m_menu = new QMenu;
    Theme::setSheet(m_menu, QStringLiteral(
        "QMenu{background:#FFFFFF;border:1px solid rgba(0,0,0,20);border-radius:10px;padding:6px;}"
        "QMenu::item{padding:7px 28px 7px 12px;border-radius:6px;color:#1C1C1E;}"
        "QMenu::item:selected{background:rgba(79,107,255,31);color:#1C1C1E;}"
        "QMenu::separator{height:1px;background:rgba(0,0,0,15);margin:5px 8px;}"
        "QMenu::icon{padding-left:8px;}"));
    buildMenu();
    connect(m_menu, &QMenu::aboutToShow, this, &App::updateMenu);

    EditRequest::setHandler([this](const QImage& img, const QPoint& pos, bool hasPos, QWidget* src) { editImage(img, pos, hasPos, src); });
    SettingsRequest::setHandler([this] {
        openSettings();
        if (m_settings) m_settings->showAiSection();
    });

    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setContextMenu(m_menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) QTimer::singleShot(120, this, [this] { startCapture(); });
    });
    m_tray->show();
    g_tray = m_tray;
    m_updater = new Updater(this);
    if (!qEnvironmentVariableIsSet("QINGYING_SMOKE_REPORT"))
        QTimer::singleShot(8000, m_updater, &Updater::maybeAutoCheck);

    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        if (m_welcomePending) {
            m_welcomePending = false;
            openSettings();
            return;
        }
        if (m_lastRecording.isEmpty()) return;
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(m_lastRecording)});
        m_lastRecording.clear();
    });
    connect(&m_hotkey, &HotkeyManager::activated, this, [this] { startCapture(); });
    connect(&m_recordHotkey, &HotkeyManager::activated, this, &App::onRecordHotkey);
    connect(&m_ocrTrHotkey, &HotkeyManager::activated, this, &App::onOcrTranslateHotkey);
    connect(&m_askHotkey, &HotkeyManager::activated, this, &App::onAskHotkey);
    const QKeySequence hk = Settings::hotkey();
    const QKeySequence rhk = Settings::recordHotkey();
    const QKeySequence thk = Settings::ocrTranslateHotkey();
    const QKeySequence ahk = Settings::askHotkey();
    const bool ok = m_hotkey.setHotkey(hk);
    const bool rok = m_recordHotkey.setHotkey(rhk);
    const bool tok = thk != hk && thk != rhk && thk != ahk && m_ocrTrHotkey.setHotkey(thk);
    const bool aok = ahk != hk && ahk != rhk && ahk != thk && m_askHotkey.setHotkey(ahk);
    updateMenu();
    const QString hkText = hk.toString(QKeySequence::NativeText);
    const QString rhkText = rhk.toString(QKeySequence::NativeText);
    const bool firstRun = !Settings::welcomed() && !qEnvironmentVariableIsSet("QINGYING_SMOKE_REPORT");
    if (ok && firstRun) {
        Settings::setWelcomed(true);
        m_welcomePending = true;
        m_tray->showMessage(QCoreApplication::translate("QingYing", "欢迎使用青影"),
                            QCoreApplication::translate("QingYing", "按 %1 截图，%2 录屏；右键托盘可钉图、查看历史。点击此提示打开设置。").arg(hkText, rhkText),
                            appIcon(), 10000);
    } else if (ok)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "青影已在后台运行"),
                            QCoreApplication::translate("QingYing", "按 %1 截图 · %2 录屏").arg(hkText, rhkText),
                            appIcon(), 3000);
    else
        m_tray->showMessage(QCoreApplication::translate("QingYing", "快捷键冲突"),
                            QCoreApplication::translate("QingYing", "%1 已被其他程序占用，请在设置中更换。也可单击托盘图标截图。").arg(hkText),
                            QSystemTrayIcon::Warning, 6000);
    if (ok && !rok)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "录屏热键冲突"),
                            QCoreApplication::translate("QingYing", "%1 已被占用，请在设置中更换录屏热键。").arg(rhkText),
                            QSystemTrayIcon::Warning, 6000);
    if (!tok)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "识别并翻译热键冲突"),
                            QCoreApplication::translate("QingYing", "%1 已被占用，请在设置中更换识别并翻译热键。").arg(thk.toString(QKeySequence::NativeText)),
                            QSystemTrayIcon::Warning, 6000);
    if (!aok)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "截图秒答热键冲突"),
                            QCoreApplication::translate("QingYing", "%1 已被占用，请在设置中更换截图秒答热键。").arg(ahk.toString(QKeySequence::NativeText)),
                            QSystemTrayIcon::Warning, 6000);
    writeSmokeReport();
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_EDIT") && qEnvironmentVariableIsSet("QINGYING_SMOKE_REPORT")) {
        // 在报告写完之后再运行（报告生成期间可能有嵌套事件循环，提前运行会与报告文件写入交错）
        const QString path = qEnvironmentVariable("QINGYING_SMOKE_REPORT");
        // 钉图编辑流程自检：编辑 → 原钉图隐藏、出现预选遮罩 → 取消 → 原钉图恢复
        QImage img(240, 160, QImage::Format_RGB32);
        img.fill(QColor(0x4F, 0x6B, 0xFF));
        auto* pin = new PinWindow(img);
        pin->placeImageAt(QPoint(200, 200));
        pin->show();
        QTimer::singleShot(600, this, [this, pin, path] {
            Diagnostics::log(QStringLiteral("smoke: edit request"));
            EditRequest::request(pin->grab().toImage(), QPoint(200, 200), true, pin);
            Diagnostics::log(QStringLiteral("smoke: edit returned overlays=%1").arg(m_overlays.size()));
            {
                QFile f(path);
                const bool during = !m_overlays.isEmpty() && !pin->isVisible();
                const QString dbg = QStringLiteral("overlays=%1 pin_visible=%2 busy=%3 handler=%4").arg(m_overlays.size()).arg(pin->isVisible()).arg(m_busy).arg(EditRequest::available());
                endCapture();
                Diagnostics::log(QStringLiteral("smoke: edit ended pin_visible=%1 path=%2").arg(pin->isVisible()).arg(path));
                if (f.open(QIODevice::Append | QIODevice::Text))
                    f.write(QStringLiteral("edit_overlay=%1 edit_restored=%2 %3\n").arg(during).arg(pin->isVisible()).arg(dbg).toUtf8());
                pin->close();
            }
        });
    }
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_QUIT_OCR")) {
        // 退出自检：OCR 进行中时退出，进程应正常结束且不崩溃
        QTimer::singleShot(500, this, [] {
            QImage img(800, 200, QImage::Format_RGB32);
            img.fill(Qt::white);
            QPainter p(&img);
            p.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 32));
            p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("quit while OCR"));
            p.end();
            Recognize::ocr(img);
            Diagnostics::log(QStringLiteral("smoke: quitting during OCR"));
            QApplication::quit();
        });
    }
}

// 托盘菜单（语言切换后重建）
void App::buildMenu()
{
    for (QAction* a : m_menu->actions())
        if (a->menu()) a->menu()->deleteLater();
    m_menu->clear();
    m_captureAct = m_menu->addAction(menuIcon(Icon::Capture), QCoreApplication::translate("QingYing", "截图"));
    connect(m_captureAct, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, [this] { startCapture(); }); });
    QMenu* delay = m_menu->addMenu(menuIcon(Icon::Clock), QCoreApplication::translate("QingYing", "延时截图"));
    for (int s : {3, 5, 10})
        delay->addAction(QCoreApplication::translate("QingYing", "%1 秒后").arg(s), this, [this, s] { startDelayedCapture(s); });
    m_timedMenu = m_menu->addMenu(menuIcon(Icon::Clock), QCoreApplication::translate("QingYing", "定时截图"));
    for (int s : {10, 30, 60, 300}) {
        QAction* a = m_timedMenu->addAction(s < 60 ? QCoreApplication::translate("QingYing", "每 %1 秒").arg(s) : QCoreApplication::translate("QingYing", "每 %1 分钟").arg(s / 60), this, [this, s] { startTimedCapture(s); });
        a->setCheckable(true);
        a->setData(s);
    }
    m_timedMenu->addSeparator();
    m_timedMenu->addAction(QCoreApplication::translate("QingYing", "停止定时截图"), this, [this] { startTimedCapture(0); })->setData(0);
    connect(m_timedMenu, &QMenu::aboutToShow, this, [this] {
        for (QAction* a : m_timedMenu->actions()) {
            if (a->isCheckable()) a->setChecked(a->data().toInt() == m_timedSeconds);
            else if (!a->isSeparator()) a->setEnabled(m_timedSeconds > 0);
        }
    });
    m_menu->addAction(menuIcon(Icon::Scroll), QCoreApplication::translate("QingYing", "长截图（滚动）"), this,
                      [this] { QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::LongShot)); }); });
    m_menu->addAction(menuIcon(Icon::Picker), QCoreApplication::translate("QingYing", "取色"), this,
                      [this] { QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Pick)); }); });
    QMenu* rec = m_menu->addMenu(menuIcon(Icon::Record), QCoreApplication::translate("QingYing", "录屏"));
    m_recordAct = rec->addAction(QCoreApplication::translate("QingYing", "录制 MP4"), this, [this] { m_recordGif = false; QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Record)); }); });
    rec->addAction(QCoreApplication::translate("QingYing", "录制 GIF"), this, [this] { m_recordGif = true; QTimer::singleShot(180, this, [this] { startCapture(int(CaptureMode::Record)); }); });
    m_menu->addSeparator();
    m_pinMenu = m_menu->addMenu(menuIcon(Icon::Pin), QCoreApplication::translate("QingYing", "钉图"));
    m_pinMenu->addAction(QCoreApplication::translate("QingYing", "全部显示"), this, [] { PinWindow::showAll(); });
    m_pinMenu->addAction(QCoreApplication::translate("QingYing", "全部隐藏"), this, [] { PinWindow::hideAll(); });
    m_pinMenu->addAction(QCoreApplication::translate("QingYing", "全部关闭"), this, [] { PinWindow::closeAll(); });
    m_recentMenu = m_menu->addMenu(menuIcon(Icon::History), QCoreApplication::translate("QingYing", "最近截图"));
    connect(m_recentMenu, &QMenu::aboutToShow, this, &App::rebuildRecentMenu);
    m_menu->addSeparator();
    m_menu->addAction(menuIcon(Icon::Settings), QCoreApplication::translate("QingYing", "设置…"), this, &App::openSettings);
    m_autoAct = m_menu->addAction(QCoreApplication::translate("QingYing", "开机自启"));
    m_autoAct->setCheckable(true);
    connect(m_autoAct, &QAction::toggled, this, [](bool on) { Settings::setAutostart(on); });
    m_menu->addAction(QCoreApplication::translate("QingYing", "检查更新"), this, [this] { m_updater->check(true); });
    m_menu->addAction(QCoreApplication::translate("QingYing", "关于青影"), this, &App::showAbout);
    m_menu->addSeparator();
    m_menu->addAction(QCoreApplication::translate("QingYing", "退出"), qApp, [this] {
        QWidget* aw = QApplication::activeWindow();
        QWidget* pop = QApplication::activePopupWidget();
        void* frames[24];
        const USHORT n = CaptureStackBackTrace(0, 24, frames, nullptr);
        QStringList st;
        for (USHORT i = 0; i < n; ++i) {
            HMODULE mod = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, static_cast<LPCWSTR>(frames[i]), &mod);
            wchar_t name[MAX_PATH] = {};
            GetModuleFileNameW(mod, name, MAX_PATH);
            st << QStringLiteral("%1+0x%2").arg(QFileInfo(QString::fromWCharArray(name)).fileName())
                      .arg(quintptr(frames[i]) - quintptr(mod), 0, 16);
        }
        Diagnostics::log(QStringLiteral("quit requested from tray menu (menuVisible=%1 active=%2 popup=%3 buttons=%4) stack=%5")
                             .arg(m_menu->isVisible())
                             .arg(aw ? QString::fromLatin1(aw->metaObject()->className()) : QStringLiteral("-"))
                             .arg(pop ? QString::fromLatin1(pop->metaObject()->className()) : QStringLiteral("-"))
                             .arg(int(QGuiApplication::mouseButtons()))
                             .arg(st.join(QLatin1Char(' '))));
        QApplication::quit();
    });
}

void App::retranslate()
{
    buildMenu();
    QApplication::setApplicationDisplayName(QCoreApplication::translate("QingYing", "青影"));
    updateMenu();
    if (m_history) { // 历史窗口重新创建以刷新文字
        const bool visible = m_history->isVisible();
        m_history->close();
        if (visible) QTimer::singleShot(0, this, &App::openHistory);
    }
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
        f.write(QStringLiteral("tray_available=%1\ntray_visible=%2\nhotkey=%3\nhotkey_registered=%4\nrecord_hotkey=%5\nrecord_hotkey_registered=%6\nscreens=%7\n")
                    .arg(QSystemTrayIcon::isSystemTrayAvailable())
                    .arg(m_tray->isVisible())
                    .arg(m_hotkey.current().toString())
                    .arg(m_hotkey.isRegistered())
                    .arg(m_recordHotkey.current().toString())
                    .arg(m_recordHotkey.isRegistered())
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
        for (int variant = 0; variant < 3; ++variant) {
            const auto fmt = variant == 0 ? Recorder::Format::Mp4 : Recorder::Format::Gif;
            const QString out = QDir::temp().filePath(fmt == Recorder::Format::Mp4 ? QStringLiteral("qy_smoke.mp4") : QStringLiteral("qy_smoke.gif"));
            QFile::remove(out);
            Recorder rec(QRect(0, 0, 320, 240), fmt, 15, true, out);
            rec.setGifDither(variant == 2);
            QString err;
            const bool started = rec.start(&err);
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            rec.stop();
            f.write(QStringLiteral("record_%1 started=%2 backend=%6 frames=%3 bytes=%4 est=%7 error=%5\n")
                        .arg(variant == 0 ? QStringLiteral("mp4") : variant == 1 ? QStringLiteral("gif") : QStringLiteral("gif_dither"))
                        .arg(started).arg(rec.framesWritten()).arg(QFileInfo(out).size()).arg(err + rec.lastError())
                        .arg(rec.backend()).arg(rec.estimatedBytes()).toUtf8());
        }
        // 可选：带麦克风的短 MP4（默认关；设置 QINGYING_SMOKE_MIC=1 启用）
        if (qEnvironmentVariableIsSet("QINGYING_SMOKE_MIC")) {
            const QString out = QDir::temp().filePath(QStringLiteral("qy_smoke_mic.mp4"));
            QFile::remove(out);
            Recorder rec(QRect(0, 0, 320, 240), Recorder::Format::Mp4, 15, true, out);
            rec.setRecordMic(true);
            QString err;
            const bool started = rec.start(&err);
            const bool capturing = rec.micCapturing();
            const QString warn = rec.micWarning();
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            rec.stop();
            f.write(QStringLiteral("record_mp4_mic started=%1 capturing=%2 warn=%3 frames=%4 bytes=%5 error=%6\n")
                        .arg(started)
                        .arg(capturing)
                        .arg(warn)
                        .arg(rec.framesWritten())
                        .arg(QFileInfo(out).size())
                        .arg(err + rec.lastError())
                        .toUtf8());
        }
    }
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_SHOTS")) {
        const QString dir = qEnvironmentVariable("QINGYING_SMOKE_SHOTS");
        QDir().mkpath(dir);
        {
            SettingsDialog sd([](const QKeySequence&) { return true; }, [](const QKeySequence&) { return true; },
                               [](const QKeySequence&) { return true; }, [](const QKeySequence&) { return true; });
            sd.resize(780, 560);
            sd.show();
            for (int i = 0; i < sd.pageCount(); ++i) {
                sd.setPage(i);
                QCoreApplication::processEvents();
                sd.grab().save(dir + QStringLiteral("/settings-%1.png").arg(i + 1));
            }
            sd.resize(sd.minimumSize());
            sd.setPage(0);
            QCoreApplication::processEvents();
            sd.grab().save(dir + QStringLiteral("/settings-min.png"));
            sd.hide();
        }
        HistoryWindow hw;
        hw.grab().save(dir + QStringLiteral("/history.png"));
        QMessageBox mb(QMessageBox::Question, QStringLiteral("QingYing"), QStringLiteral("Qt dialog buttons"),
                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        mb.grab().save(dir + QStringLiteral("/msgbox.png"));
        m_menu->grab().save(dir + QStringLiteral("/tray.png"));
        f.write(QStringLiteral("shots=%1\n").arg(dir).toUtf8());
    }
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_UPDATE")) {
        QNetworkAccessManager net;
        QNetworkRequest req(QUrl(QString::fromLatin1(Updater::kLatestApi)));
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QingYing-smoke"));
        QNetworkReply* rep = net.get(req);
        QEventLoop loop;
        QObject::connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(20000, &loop, &QEventLoop::quit);
        loop.exec();
        Updater::Release rel;
        const bool parsed = Updater::parse(rep->readAll(), &rel);
        f.write(QStringLiteral("tls_backends=%1 active=%2\nupdate_http=%3 err=%4 parsed=%5 tag=%6 setup=%7 sums=%8 portable=%9\n")
                    .arg(QSslSocket::availableBackends().join(QLatin1Char(',')), QSslSocket::activeBackend())
                    .arg(rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                    .arg(rep->errorString()).arg(parsed).arg(rel.tag, rel.setupName)
                    .arg(!rel.sumsUrl.isEmpty()).arg(Updater::isPortable()).toUtf8());
        rep->deleteLater();
    }
    {
        const QString enc = Secret::protect(QStringLiteral("Authorization: Bearer 测试"));
        f.write(QStringLiteral("dpapi_roundtrip=%1 plaintext_in_cipher=%2\n")
                    .arg(Secret::unprotect(enc) == QStringLiteral("Authorization: Bearer 测试"))
                    .arg(enc.contains(QLatin1String("Bearer"))).toUtf8());
    }
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_UPLOAD")) {
        Uploads::Config c;
        c.url = QStringLiteral("https://httpbin.org/post");
        c.field = QStringLiteral("file");
        c.jsonPath = QStringLiteral("url");
        c.secret = QStringLiteral("X-Test: qingying");
        QImage img(64, 64, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QString url, err;
        QEventLoop loop;
        Uploads::upload(img, c, [&](const QString& u, const QString& e) { url = u; err = e; loop.quit(); });
        QTimer::singleShot(30000, &loop, &QEventLoop::quit);
        loop.exec();
        f.write(QStringLiteral("upload_url=%1 err=%2\n").arg(url, err).toUtf8());
    }
    {
        // 运行时切换语言：重建托盘菜单后第一项文字应随之变化，再切回
        const QString orig = Language::current();
        const QString other = orig == QLatin1String("en_US") ? QStringLiteral("zh_CN") : QStringLiteral("en_US");
        Language::apply(other);
        retranslate();
        const QString switched = m_menu->actions().value(0) ? m_menu->actions().first()->text() : QString();
        Language::apply(orig);
        retranslate();
        const QString back = m_menu->actions().value(0) ? m_menu->actions().first()->text() : QString();
        f.write(QStringLiteral("lang_switch=%1>%2:%3>%4:%5 actions=%6\n").arg(orig, other, switched, orig, back).arg(m_menu->actions().size()).toUtf8());
    }
    f.write(QStringLiteral("lang=%1 sample=%2\n").arg(Settings::effectiveLanguage(), QCoreApplication::translate("QingYing", "设置")).toUtf8());
    f.write(QStringLiteral("theme=%1 dark=%2 system_dark=%3\n").arg(Settings::theme()).arg(Theme::isDark()).arg(Theme::systemIsDark()).toUtf8());
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
    m_captureAct->setText(QCoreApplication::translate("QingYing", "截图\t%1").arg(Settings::hotkey().toString(QKeySequence::NativeText)));
    if (m_recordAct)
        m_recordAct->setText(QCoreApplication::translate("QingYing", "录制 MP4\t%1").arg(Settings::recordHotkey().toString(QKeySequence::NativeText)));
    const int pins = PinWindow::all().size();
    m_pinMenu->setTitle(pins ? QCoreApplication::translate("QingYing", "钉图（%1）").arg(pins) : QCoreApplication::translate("QingYing", "钉图"));
    m_pinMenu->setEnabled(pins > 0);
    const QSignalBlocker b(m_autoAct);
    m_autoAct->setChecked(Settings::autostart());
    m_tray->setToolTip(QCoreApplication::translate("QingYing", "青影 · 截图 %1 · 录屏 %2")
                           .arg(Settings::hotkey().toString(QKeySequence::NativeText),
                                Settings::recordHotkey().toString(QKeySequence::NativeText)));
}

bool App::applyHotkey(const QKeySequence& seq)
{
    if (seq == Settings::recordHotkey() || seq == m_recordHotkey.current()) return false;
    if (seq == Settings::ocrTranslateHotkey() || seq == m_ocrTrHotkey.current()) return false;
    if (seq == Settings::askHotkey() || seq == m_askHotkey.current()) return false;
    const QKeySequence old = m_hotkey.current();
    if (m_hotkey.isRegistered() && old == seq) return true;
    if (m_hotkey.setHotkey(seq)) {
        updateMenu();
        return true;
    }
    if (!old.isEmpty()) m_hotkey.setHotkey(old);
    return false;
}

bool App::applyRecordHotkey(const QKeySequence& seq)
{
    if (seq == Settings::hotkey() || seq == m_hotkey.current()) return false;
    if (seq == Settings::ocrTranslateHotkey() || seq == m_ocrTrHotkey.current()) return false;
    if (seq == Settings::askHotkey() || seq == m_askHotkey.current()) return false;
    const QKeySequence old = m_recordHotkey.current();
    if (m_recordHotkey.isRegistered() && old == seq) return true;
    if (m_recordHotkey.setHotkey(seq)) {
        updateMenu();
        return true;
    }
    if (!old.isEmpty()) m_recordHotkey.setHotkey(old);
    return false;
}

bool App::applyOcrTranslateHotkey(const QKeySequence& seq)
{
    if (seq == Settings::hotkey() || seq == m_hotkey.current()) return false;
    if (seq == Settings::recordHotkey() || seq == m_recordHotkey.current()) return false;
    if (seq == Settings::askHotkey() || seq == m_askHotkey.current()) return false;
    const QKeySequence old = m_ocrTrHotkey.current();
    if (m_ocrTrHotkey.isRegistered() && old == seq) return true;
    if (m_ocrTrHotkey.setHotkey(seq)) return true;
    if (!old.isEmpty()) m_ocrTrHotkey.setHotkey(old);
    return false;
}

void App::onOcrTranslateHotkey()
{
    for (const auto& o : m_overlays) {
        if (o && o->recognizeAndTranslate()) return;
    }
    if (m_tray)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "识别并翻译"),
                            QCoreApplication::translate("QingYing", "请先框选区域，再按 %1。").arg(Settings::ocrTranslateHotkey().toString(QKeySequence::NativeText)),
                            appIcon(), 3000);
}

bool App::applyAskHotkey(const QKeySequence& seq)
{
    if (seq == Settings::hotkey() || seq == m_hotkey.current()) return false;
    if (seq == Settings::recordHotkey() || seq == m_recordHotkey.current()) return false;
    if (seq == Settings::ocrTranslateHotkey() || seq == m_ocrTrHotkey.current()) return false;
    const QKeySequence old = m_askHotkey.current();
    if (m_askHotkey.isRegistered() && old == seq) return true;
    if (m_askHotkey.setHotkey(seq)) return true;
    if (!old.isEmpty()) m_askHotkey.setHotkey(old);
    return false;
}

void App::onAskHotkey()
{
    for (const auto& o : m_overlays) {
        if (o && o->askSelection()) return;
    }
    if (m_tray)
        m_tray->showMessage(QCoreApplication::translate("QingYing", "截图秒答"),
                            QCoreApplication::translate("QingYing", "请先框选区域，再按 %1。").arg(Settings::askHotkey().toString(QKeySequence::NativeText)),
                            appIcon(), 3000);
}

void App::onRecordHotkey()
{
    if (m_recordSession) {
        m_recordSession->requestStop();
        return;
    }
    if (m_busy || !m_overlays.isEmpty()) return;
    m_recordGif = false;
    startCapture(int(CaptureMode::Record));
}

void App::startDelayedCapture(int seconds)
{
    m_tray->showMessage(QCoreApplication::translate("QingYing", "延时截图"), QCoreApplication::translate("QingYing", "%1 秒后开始截图").arg(seconds), appIcon(), 1500);
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
    m_recordSession = s;
    connect(s, &QObject::destroyed, this, [this] {
        m_busy = false;
        m_recordSession.clear();
    });
    connect(s, &RecordSession::finished, this, [this](bool ok, const QString& path, const QString& err) {
        if (ok) {
            m_lastRecording = path;
            m_tray->showMessage(QCoreApplication::translate("QingYing", "录制完成"), QCoreApplication::translate("QingYing", "%1\n点击打开所在文件夹").arg(QDir::toNativeSeparators(path)),
                                appIcon(), 4000);
        } else {
            m_tray->showMessage(QCoreApplication::translate("QingYing", "录制失败"), err, QSystemTrayIcon::Warning, 5000);
        }
    });
    QString err;
    if (!s->begin(&err)) {
        m_tray->showMessage(QCoreApplication::translate("QingYing", "无法开始录制"), err, QSystemTrayIcon::Warning, 5000);
        s->close();
    } else if (!s->micWarning().isEmpty()) {
        m_tray->showMessage(QCoreApplication::translate("QingYing", "麦克风不可用"), s->micWarning(), QSystemTrayIcon::Warning, 5000);
    }
}

void App::onDelivered(const QImage& image, const QString& path)
{
    History::add(image);
    if (!path.isEmpty())
        m_tray->showMessage(QCoreApplication::translate("QingYing", "截图已保存"), QDir::toNativeSeparators(path), appIcon(), 2500);
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
                                            QCoreApplication::translate("QingYing", "%1:%2:%3  %4×%5  复制")
                                                .arg(stamp.mid(9, 2), stamp.mid(11, 2), stamp.mid(13, 2))
                                                .arg(full.width()).arg(full.height()));
        connect(act, &QAction::triggered, this, [path] { QGuiApplication::clipboard()->setImage(QImage(path)); });
    }
    if (list.isEmpty()) m_recentMenu->addAction(QCoreApplication::translate("QingYing", "暂无记录"))->setEnabled(false);
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(QCoreApplication::translate("QingYing", "历史记录…"), this, &App::openHistory);
    m_recentMenu->addAction(QCoreApplication::translate("QingYing", "打开保存文件夹"), this, [] {
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
    // 编辑模式：把待编辑图片画到目标屏幕的冻结画面上，并预先选中该区域
    int editIndex = -1;
    QRect editLocal;
    if (m_edit.pending) {
        m_edit.pending = false;
        const QPoint anchor = m_edit.hasPos ? m_edit.pos : QCursor::pos();
        for (int i = 0; i < shots.size(); ++i)
            if (shots[i].screen->geometry().contains(anchor)) editIndex = i;
        if (editIndex < 0 && !shots.isEmpty()) editIndex = 0;
        if (editIndex >= 0) {
            Shot& sh = shots[editIndex];
            const qreal dpr = sh.image.devicePixelRatio();
            QImage img = m_edit.image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            img.setDevicePixelRatio(1);
            const QSize monSize = sh.image.size();
            if (img.width() > monSize.width() || img.height() > monSize.height()) // 比屏幕大：缩小到可完整显示
                img = img.scaled(monSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPoint local = m_edit.hasPos ? ((anchor - sh.screen->geometry().topLeft()) * dpr)
                                         : QPoint((monSize.width() - img.width()) / 2, (monSize.height() - img.height()) / 2);
            local.setX(qBound(0, local.x(), monSize.width() - img.width()));
            local.setY(qBound(0, local.y(), monSize.height() - img.height()));
            editLocal = QRect(local, img.size());
            QImage canvas = sh.image;
            canvas.setDevicePixelRatio(1);
            {
                QPainter p(&canvas);
                p.drawImage(local, img);
            }
            canvas.setDevicePixelRatio(dpr);
            sh.image = canvas;
        }
    }
    CaptureOverlay* under = nullptr;
    for (int idx = 0; idx < shots.size(); ++idx) {
        const Shot& sh = shots[idx];
        auto* o = new CaptureOverlay(sh.screen, sh.image, sh.monitor, &m_detector);
        connect(o, &CaptureOverlay::finished, this, &App::endCapture, Qt::QueuedConnection);
        o->setMode(CaptureMode(mode));
        connect(o, &CaptureOverlay::colorPicked, this, [this](const QString& t) {
            m_tray->showMessage(QCoreApplication::translate("QingYing", "已复制颜色"), t, appIcon(), 1500);
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
        connect(o, &CaptureOverlay::controlsModeChanged, this, [this](bool controls) {
            for (auto& other : m_overlays)
                if (other) other->setControlsMode(controls);
        });
        m_overlays.append(o);
        o->show();
        if (editIndex >= 0) {
            if (idx == editIndex) {
                connect(o, &CaptureOverlay::pinned, this, [this] { m_edit.pinned = true; });
                o->presetSelection(editLocal, !m_edit.source.isNull());
                under = o;
            }
        } else if (sh.screen->geometry().contains(QCursor::pos())) {
            under = o;
        }
    }
    if (!under && !m_overlays.isEmpty()) under = m_overlays.first();
    if (under) under->activate();
}

void App::editImage(const QImage& image, const QPoint& pos, bool hasPos, QWidget* source)
{
    Diagnostics::log(QStringLiteral("edit begin"));
    if (!m_overlays.isEmpty() || m_busy || image.isNull()) {
        Diagnostics::log(QStringLiteral("edit refused: overlays=%1 busy=%2 null=%3").arg(m_overlays.size()).arg(m_busy).arg(image.isNull()));
        return;
    }
    m_edit = Edit();
    m_edit.pending = true;
    m_edit.image = image;
    m_edit.pos = pos;
    m_edit.hasPos = hasPos;
    m_edit.source = source;
    // 冻结画面前先隐藏原钉图 / 菜单：等待 DWM 合成完成两帧，确保屏幕上已不再显示它们（不依赖固定延时）
    if (source) source->hide();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    DwmFlush();
    DwmFlush();
    startCapture();
    if (m_overlays.isEmpty()) { // 未能进入编辑（如设置窗口打开中）：恢复原状
        Diagnostics::log(QStringLiteral("edit: capture did not start"));
        if (m_edit.source) m_edit.source->show();
        m_edit = Edit();
    }
}

void App::startTimedCapture(int seconds)
{
    m_timedSeconds = seconds;
    if (!m_timed) {
        m_timed = new QTimer(this);
        connect(m_timed, &QTimer::timeout, this, &App::timedTick);
    }
    if (seconds <= 0) {
        m_timed->stop();
        m_tray->setToolTip(QCoreApplication::translate("QingYing", "青影"));
        return;
    }
    m_timed->start(seconds * 1000);
    const QString every = seconds < 60 ? QCoreApplication::translate("QingYing", "每 %1 秒").arg(seconds)
                                       : QCoreApplication::translate("QingYing", "每 %1 分钟").arg(seconds / 60);
    m_tray->setToolTip(QCoreApplication::translate("QingYing", "青影 · 定时截图（%1）").arg(every));
    m_tray->showMessage(QCoreApplication::translate("QingYing", "定时截图已开始"),
                        QCoreApplication::translate("QingYing", "%1保存一次全屏截图到 %2").arg(every, QDir::toNativeSeparators(Settings::saveDir())),
                        appIcon(), 3000);
}

void App::timedTick()
{
    if (!m_overlays.isEmpty()) return; // 正在截图时跳过本次
    const QList<QScreen*> screens = QGuiApplication::screens();
    const QString base = Settings::defaultFilePath();
    for (int i = 0; i < screens.size(); ++i) {
        const QImage img = ScreenGrabber::grab(ScreenGrabber::monitorRect(screens[i]));
        if (img.isNull()) continue;
        QFileInfo fi(base);
        QString path = screens.size() > 1 ? fi.dir().filePath(QStringLiteral("%1_%2.%3").arg(fi.completeBaseName()).arg(i + 1).arg(fi.suffix())) : base;
        for (int n = 2; QFileInfo::exists(path); ++n) // 同一秒内多次截图不覆盖
            path = fi.dir().filePath(QStringLiteral("%1 (%2).%3").arg(QFileInfo(path).completeBaseName().section(QStringLiteral(" ("), 0, 0)).arg(n).arg(fi.suffix()));
        if (Settings::saveImage(img, path)) History::add(img);
    }
    if (m_history) m_history->reload();
}

void App::endCapture()
{
    if (m_edit.source) { // 编辑钉图结束：钉了新图则关闭旧钉图，否则恢复显示
        if (m_edit.pinned) m_edit.source->close();
        else m_edit.source->show();
    }
    m_edit = Edit();
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
    m_settings = new SettingsDialog([this](const QKeySequence& k) { return applyHotkey(k); },
                                    [this](const QKeySequence& k) { return applyRecordHotkey(k); },
                                    [this](const QKeySequence& k) { return applyOcrTranslateHotkey(k); },
                                    [this](const QKeySequence& k) { return applyAskHotkey(k); });
    m_settings->setAttribute(Qt::WA_DeleteOnClose);
    m_settings->setCheckUpdate([this] { m_updater->check(true); });
    m_settings->setSuspendHotkey([this](bool on) {
        if (on) {
            m_hotkey.unregister();
            m_recordHotkey.unregister();
            m_ocrTrHotkey.unregister();
            m_askHotkey.unregister();
        } else {
            if (!m_hotkey.isRegistered()) m_hotkey.setHotkey(Settings::hotkey());
            if (!m_recordHotkey.isRegistered()) m_recordHotkey.setHotkey(Settings::recordHotkey());
            if (!m_ocrTrHotkey.isRegistered()) m_ocrTrHotkey.setHotkey(Settings::ocrTranslateHotkey());
            if (!m_askHotkey.isRegistered()) m_askHotkey.setHotkey(Settings::askHotkey());
        }
    });
    connect(m_settings, &SettingsDialog::changed, this, &App::updateMenu);
    connect(m_settings, &QDialog::finished, this, [] { History::prune(); });
    connect(m_settings, &SettingsDialog::languageChanged, this, [this] {
        // 即时切换语言：托盘菜单重建，设置窗口在同一位置、同一页重新打开
        const QPointer<SettingsDialog> old = m_settings;
        const int page = old->page();
        const QRect geo = old->geometry();
        QTimer::singleShot(0, this, [this, old, page, geo] {
            if (old) old->close();
            m_settings = nullptr;
            retranslate();
            openSettings();
            m_settings->setGeometry(geo);
            m_settings->setPage(page);
        });
    });
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void App::showAbout()
{
    QMessageBox box;
    box.setWindowTitle(QCoreApplication::translate("QingYing", "关于"));
    box.setIconPixmap(appIcon().pixmap(64, 64));
    box.setText(QCoreApplication::translate("QingYing", "<h3>青影 QingYing %1</h3>").arg(QApplication::applicationVersion()));
    box.setInformativeText(QCoreApplication::translate("QingYing", "简洁好看的 Windows 截图工具。<br>区域截图 · 标注 · 钉图<br><br>"
                                          "基于 Qt %1 · MIT 许可证").arg(QString::fromLatin1(qVersion())));
    box.exec();
}
