#pragma once
#include <QImage>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class HotkeyManager;

// 滚动长截图：自动向选区发送滚轮，按重叠区域拼接；单击鼠标 / Esc / 到底自动停止
class ScrollCapture : public QWidget {
    Q_OBJECT
public:
    explicit ScrollCapture(const QRect& physicalRegion);
    ~ScrollCapture() override;
    void start();
    // 在 prev 与 cur 之间寻找垂直位移（cur 相对 prev 向上滚动的像素数），失败返回 -1
    static int findShift(const QImage& prev, const QImage& cur);

signals:
    void delivered(const QImage& image, const QString& savedPath);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    void step();
    void stop(const QString& reason);
    void finish(const QString& action);
    void updatePreview();

    QRect m_region;
    QImage m_result;
    QImage m_last;
    QTimer m_timer;
    int m_stillCount = 0;
    int m_failCount = 0;
    bool m_running = false;
    bool m_waitRender = false;
    QWidget* m_frame = nullptr;
    QLabel* m_preview;
    QLabel* m_status;
    QPushButton* m_stop;
    QWidget* m_actions;
    HotkeyManager* m_esc;
};
