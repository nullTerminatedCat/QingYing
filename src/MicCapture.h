#pragma once
#include <QString>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

// WASAPI 共享模式采集默认麦克风，输出交错 int16 PCM。失败不抛异常。
class MicCapture {
public:
    ~MicCapture();
    bool open(QString* error); // 打开设备并启动采集线程；失败返回 false 并写友好错误
    void stop();
    void setMuted(bool on) { m_muted = on; }
    bool isMuted() const { return m_muted; }
    bool isOpen() const { return m_open; }
    int sampleRate() const { return m_rate; }
    int channels() const { return m_channels; }
    // 取出自 open 以来累积的 PCM（不含静音丢弃的段）；frames 为每声道采样数
    void drain(std::vector<int16_t>* interleaved, uint32_t* frames);

private:
    void run();
    void appendPcm(const int16_t* data, uint32_t frames);
    std::thread m_thread;
    std::mutex m_mutex;
    std::vector<int16_t> m_buf;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_muted{false};
    bool m_open = false;
    int m_rate = 48000;
    int m_channels = 1;
};
