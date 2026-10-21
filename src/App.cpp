#include "App.h"
#include "CaptureOverlay.h"
#include "Icons.h"
#include "ScreenGrabber.h"
#include "Settings.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QSystemTrayIcon>
#include <QTimer>

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
    connect(m_captureAct, &QAction::triggered, this, [this] { QTimer::singleShot(180, this, &App::startCapture); });
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
        if (r == QSystemTrayIcon::Trigger) QTimer::singleShot(120, this, &App::startCapture);
    });
    m_tray->show();

    connect(&m_hotkey, &HotkeyManager::activated, this, &App::startCapture);
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
}

void App::updateMenu()
{
    m_captureAct->setText(QStringLiteral("截图\t%1").arg(Settings::hotkey().toString(QKeySequence::NativeText)));
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

void App::startCapture()
{
    if (!m_overlays.isEmpty()) return;
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
    connect(m_settings, &QDialog::finished, this, &App::updateMenu);
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
