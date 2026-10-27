#pragma once
#include "Recorder.h"

#include <QTimer>
#include <QWidget>

class QLabel;
class QToolButton;
class HotkeyManager;

// 录屏会话：选区虚线框 + 悬浮迷你控制条（暂停 / 停止 / 计时），二者都不会被录进视频
class RecordSession : public QWidget {
    Q_OBJECT
public:
    RecordSession(const QRect& physicalRegion, Recorder::Format format);
    ~RecordSession() override;
    bool begin(QString* error);
    QString micWarning() const; // 请求麦克风但未能采集时的提示（可空）

signals:
    void finished(bool ok, const QString& path, const QString& error);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;

private:
    void finish();
    void refresh();
    Recorder* m_rec;
    QWidget* m_frame;
    QLabel* m_dot = nullptr;
    QLabel* m_time;
    QToolButton* m_pause;
    QToolButton* m_mic = nullptr; // 仅 MP4：显示/切换麦克风静音
    QToolButton* m_stop;
    QTimer m_tick;
    Recorder::Format m_format;
    HotkeyManager* m_esc;
    QPoint m_drag;
    bool m_done = false;
};
