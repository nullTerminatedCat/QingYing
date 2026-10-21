#pragma once
#include "HotkeyManager.h"
#include "WindowDetector.h"

#include <QList>
#include <QObject>
#include <QPointer>

class CaptureOverlay;
class QAction;
class QMenu;
class QSystemTrayIcon;
class SettingsDialog;

class App : public QObject {
    Q_OBJECT
public:
    App();
    ~App() override;

    void startCapture();
    void openSettings();
    void showAbout();

private:
    bool applyHotkey(const QKeySequence& seq);
    void updateMenu();
    void endCapture();
    void writeSmokeReport();

    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
    QAction* m_captureAct;
    QAction* m_autoAct;
    HotkeyManager m_hotkey;
    WindowDetector m_detector;
    QList<QPointer<CaptureOverlay>> m_overlays;
    QPointer<SettingsDialog> m_settings;
};
