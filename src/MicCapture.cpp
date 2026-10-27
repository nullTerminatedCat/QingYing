#include "MicCapture.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstring>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

namespace {
template <class T>
void releaseCom(T*& p)
{
    if (p) p->Release();
    p = nullptr;
}

void floatToPcm16(const float* src, int16_t* dst, UINT32 frames, int channels, WORD srcChannels)
{
    for (UINT32 i = 0; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            float v;
            if (srcChannels == 1) v = src[i];
            else if (c < srcChannels) v = src[i * srcChannels + c];
            else v = src[i * srcChannels]; // 多余声道复制左声道
            v = std::clamp(v, -1.0f, 1.0f);
            dst[i * channels + c] = int16_t(std::lrintf(v * 32767.0f));
        }
    }
}

void pcm16Copy(const int16_t* src, int16_t* dst, UINT32 frames, int channels, WORD srcChannels, WORD srcBits)
{
    if (srcBits == 16 && srcChannels == channels) {
        memcpy(dst, src, size_t(frames) * channels * sizeof(int16_t));
        return;
    }
    for (UINT32 i = 0; i < frames; ++i) {
        for (int c = 0; c < channels; ++c) {
            int16_t v = 0;
            if (srcBits == 16) {
                if (srcChannels == 1) v = src[i];
                else if (c < srcChannels) v = src[i * srcChannels + c];
                else v = src[i * srcChannels];
            } else if (srcBits == 32) {
                const int32_t* s32 = reinterpret_cast<const int32_t*>(src);
                int32_t raw = (srcChannels == 1) ? s32[i] : s32[i * srcChannels + std::min<int>(c, srcChannels - 1)];
                v = int16_t(raw >> 16);
            }
            dst[i * channels + c] = v;
        }
    }
}
}

MicCapture::~MicCapture() { stop(); }

bool MicCapture::open(QString* error)
{
    if (m_open) return true;
    m_stop = false;
    m_muted = false;
    m_buf.clear();

    // 在采集线程里完成 COM / WASAPI 初始化，便于拿到真实错误
    std::mutex readyMu;
    std::condition_variable readyCv;
    bool ready = false;
    bool ok = false;
    QString err;

    m_thread = std::thread([&, this] {
        const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool com = (co == S_OK || co == S_FALSE);
        IMMDeviceEnumerator* en = nullptr;
        IMMDevice* dev = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* capture = nullptr;
        WAVEFORMATEX* mix = nullptr;

        auto fail = [&](const QString& msg) {
            err = msg;
            ok = false;
            {
                std::lock_guard lock(readyMu);
                ready = true;
            }
            readyCv.notify_one();
            if (mix) CoTaskMemFree(mix);
            releaseCom(capture);
            releaseCom(client);
            releaseCom(dev);
            releaseCom(en);
            if (com) CoUninitialize();
        };

        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                    reinterpret_cast<void**>(&en)))) {
            fail(QCoreApplication::translate("QingYing", "无法初始化音频设备枚举"));
            return;
        }
        HRESULT hr = en->GetDefaultAudioEndpoint(eCapture, eConsole, &dev);
        if (FAILED(hr)) {
            fail(QCoreApplication::translate("QingYing", "未找到麦克风，请连接设备或在系统声音设置中选择输入设备"));
            return;
        }
        if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client)))) {
            fail(QCoreApplication::translate("QingYing", "无法打开麦克风"));
            return;
        }
        if (FAILED(client->GetMixFormat(&mix)) || !mix) {
            fail(QCoreApplication::translate("QingYing", "无法读取麦克风格式"));
            return;
        }
        // 输出统一为 16-bit PCM；声道最多保留 2
        m_rate = int(mix->nSamplesPerSec);
        m_channels = mix->nChannels >= 2 ? 2 : 1;
        const REFERENCE_TIME bufDuration = 10000000; // 1 秒缓冲
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufDuration, 0, mix, nullptr);
        if (hr == E_ACCESSDENIED || HRESULT_CODE(hr) == ERROR_ACCESS_DENIED) {
            fail(QCoreApplication::translate("QingYing", "没有麦克风权限，请在系统设置 → 隐私 → 麦克风中允许应用访问"));
            return;
        }
        if (FAILED(hr)) {
            fail(QCoreApplication::translate("QingYing", "麦克风初始化失败（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
            return;
        }
        if (FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture)))) {
            fail(QCoreApplication::translate("QingYing", "无法开始麦克风采集"));
            return;
        }
        if (FAILED(client->Start())) {
            fail(QCoreApplication::translate("QingYing", "麦克风启动失败"));
            return;
        }

        m_open = true;
        ok = true;
        {
            std::lock_guard lock(readyMu);
            ready = true;
        }
        readyCv.notify_one();

        const WORD srcCh = mix->nChannels;
        const WORD srcBits = mix->wBitsPerSample;
        const bool isFloat = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
            || (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->wBitsPerSample == 32);

        std::vector<int16_t> tmp;
        while (!m_stop) {
            UINT32 packet = 0;
            hr = capture->GetNextPacketSize(&packet);
            if (FAILED(hr)) break;
            if (packet == 0) {
                Sleep(5);
                continue;
            }
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;
            if (frames > 0 && !(flags & AUDCLNT_BUFFERFLAGS_SILENT) && !m_muted && data) {
                tmp.resize(size_t(frames) * m_channels);
                if (isFloat)
                    floatToPcm16(reinterpret_cast<const float*>(data), tmp.data(), frames, m_channels, srcCh);
                else
                    pcm16Copy(reinterpret_cast<const int16_t*>(data), tmp.data(), frames, m_channels, srcCh, srcBits);
                appendPcm(tmp.data(), frames);
            }
            capture->ReleaseBuffer(frames);
        }

        client->Stop();
        CoTaskMemFree(mix);
        releaseCom(capture);
        releaseCom(client);
        releaseCom(dev);
        releaseCom(en);
        m_open = false;
        if (com) CoUninitialize();
    });

    {
        std::unique_lock lock(readyMu);
        readyCv.wait(lock, [&] { return ready; });
    }
    if (!ok) {
        if (m_thread.joinable()) m_thread.join();
        if (error) *error = err;
        return false;
    }
    return true;
}

void MicCapture::stop()
{
    m_stop = true;
    if (m_thread.joinable()) m_thread.join();
    m_open = false;
}

void MicCapture::appendPcm(const int16_t* data, uint32_t frames)
{
    std::lock_guard lock(m_mutex);
    m_buf.insert(m_buf.end(), data, data + size_t(frames) * m_channels);
    // 防止暂停时或编码跟不上时无限积压（约 5 秒）
    const size_t cap = size_t(m_rate) * m_channels * 5;
    if (m_buf.size() > cap) m_buf.erase(m_buf.begin(), m_buf.end() - ptrdiff_t(cap));
}

void MicCapture::drain(std::vector<int16_t>* interleaved, uint32_t* frames)
{
    std::lock_guard lock(m_mutex);
    const uint32_t f = uint32_t(m_buf.size() / size_t(std::max(1, m_channels)));
    interleaved->swap(m_buf);
    m_buf.clear();
    // swap 后 interleaved 可能含不完整尾声道，截断到整帧
    interleaved->resize(size_t(f) * m_channels);
    *frames = f;
}
