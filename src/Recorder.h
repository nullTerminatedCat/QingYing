#pragma once
#include <QObject>
#include <QRect>
#include <QString>
#include <atomic>
#include <memory>
#include <thread>

// 区域录屏：Desktop Duplication 抓帧（不可用时回退 GDI BitBlt），编码为 MP4（Media Foundation H.264）或 GIF（gif.h）
class Recorder : public QObject {
    Q_OBJECT
public:
    enum class Format { Mp4, Gif };
    Recorder(const QRect& physicalRegion, Format format, int fps, bool cursor, const QString& outputPath);
    ~Recorder() override;

    bool start(QString* error);
    void stop(); // 阻塞直到文件写完
    void setPaused(bool p) { m_paused = p; }
    bool isPaused() const { return m_paused; }
    qint64 elapsedMs() const { return m_elapsed; }
    QString outputPath() const { return m_path; }
    QString lastError() const { return m_error; }
    int framesWritten() const { return m_frames; }
    qint64 estimatedBytes() const { return m_bytes; }
    QString backend() const { return m_backend; } // dxgi / gdi / dxgi+gdi，仅在 stop() 后读取

private:
    void run();
    QRect m_region;
    Format m_format;
    int m_fps;
    bool m_cursor;
    QString m_path;
    QString m_error;
    QString m_backend;
    std::atomic<qint64> m_bytes{0};
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_paused{false};
    std::atomic<qint64> m_elapsed{0};
    std::atomic<int> m_frames{0};
    std::atomic<int> m_started{0}; // 0 等待 1 成功 -1 失败
};
