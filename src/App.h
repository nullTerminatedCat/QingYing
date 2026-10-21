#pragma once
#include "HotkeyManager.h"
#include "WindowDetector.h"

#include <QList>
#include <QObject>
#include <QPointer>

class CaptureOverlay;
class HistoryWindow;
class QAction;
class QImage;
class QMenu;
class QSystemTrayIcon;
class SettingsDialog;

class App : public QObject {
    Q_OBJECT
public:
    App();
    ~App() override;

    void startCapture(int mode = 0);
    void startDelayedCapture(int seconds);
    void startLongShot(const QRect& physicalRegion);
    void openSettings();
    void openHistory();
    void showAbout();

private:
    bool applyHotkey(const QKeySequence& seq);
    void updateMenu();
    void rebuildRecentMenu();
    void endCapture();
    void onDelivered(const QImage& image, const QString& savedPath);
    void writeSmokeReport();

    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
    QMenu* m_pinMenu;
    QMenu* m_recentMenu;
    QAction* m_captureAct;
    QAction* m_autoAct;
    HotkeyManager m_hotkey;
    WindowDetector m_detector;
    QList<QPointer<CaptureOverlay>> m_overlays;
    QPointer<SettingsDialog> m_settings;
    QPointer<HistoryWindow> m_history;
    bool m_busy = false;
};
