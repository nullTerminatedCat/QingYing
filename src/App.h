#pragma once
#include "HotkeyManager.h"
#include "WindowDetector.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>

class CaptureOverlay;
class HistoryWindow;
class QAction;
class QTimer;
class QWidget;
class QMenu;
class QSystemTrayIcon;
class RecordSession;
class SettingsDialog;
class Updater;

class App : public QObject {
    Q_OBJECT
public:
    App();
    ~App() override;

    void startCapture(int mode = 0);
    // 在编辑器中重新标注图片（钉图 / 历史记录）
    void editImage(const QImage& image, const QPoint& logicalTopLeft, bool hasPosition, QWidget* source);
    void startTimedCapture(int seconds); // 0 停止
    void startDelayedCapture(int seconds);
    void startLongShot(const QRect& physicalRegion);
    void startRecording(const QRect& physicalRegion);
    void openSettings();
    void openHistory();
    void showAbout();

private:
    bool applyHotkey(const QKeySequence& seq);
    bool applyRecordHotkey(const QKeySequence& seq);
    void onRecordHotkey();
    void updateMenu();
    void buildMenu();
    void retranslate();
    void rebuildRecentMenu();
    void endCapture();
    void onDelivered(const QImage& image, const QString& savedPath);
    void writeSmokeReport();

    QSystemTrayIcon* m_tray;
    QMenu* m_menu;
    QMenu* m_pinMenu;
    QMenu* m_recentMenu;
    QAction* m_captureAct;
    QAction* m_recordAct = nullptr;
    QAction* m_autoAct;
    HotkeyManager m_hotkey;
    HotkeyManager m_recordHotkey;
    WindowDetector m_detector;
    QList<QPointer<CaptureOverlay>> m_overlays;
    QPointer<SettingsDialog> m_settings;
    QPointer<HistoryWindow> m_history;
    bool m_busy = false;
    Updater* m_updater = nullptr;
    bool m_recordGif = false;
    QPointer<RecordSession> m_recordSession;
    QString m_lastRecording;
    bool m_welcomePending = false;
    struct Edit {
        bool pending = false;
        QImage image;
        QPoint pos;
        bool hasPos = false;
        QPointer<QWidget> source;
        bool pinned = false;
    } m_edit;
    QTimer* m_timed = nullptr;
    int m_timedSeconds = 0;
    QMenu* m_timedMenu = nullptr;
    void timedTick();
};
