#define _CRT_SECURE_NO_WARNINGS
#include <QCoreApplication>
#include "Recorder.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUuid>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <vector>
#include <cstdint>
#include "CursorBlend.h"
#include "MicCapture.h"
#include "GifPalette.h"
#include <mutex>

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma warning(push, 0)
#include "../third_party/gif/gif.h"
#pragma warning(pop)

namespace {
template <class T> void releaseCom(T*& p)
{
    if (p) p->Release();
    p = nullptr;
}

class Sink {
public:
    virtual ~Sink() = default;
    virtual bool open(int w, int h, int fps, const QString& path, QString* err) = 0;
    virtual bool write(const uchar* bgra, int stride, qint64 ms) = 0;
    virtual bool writeAudio(const int16_t* pcm, uint32_t frames, int channels, int rate, qint64 ms) { Q_UNUSED(pcm); Q_UNUSED(frames); Q_UNUSED(channels); Q_UNUSED(rate); Q_UNUSED(ms); return true; }
    virtual bool close(QString* err) = 0;
    virtual qint64 bytes(qint64 ms) const = 0; // 当前文件大小估计
    virtual void enableAudio(int /*rate*/, int /*channels*/) {}
    virtual bool hasAudio() const { return false; }
};

class Mp4Sink : public Sink {
public:
    void enableAudio(int rate, int channels) override
    {
        m_wantAudio = true;
        m_ar = rate > 0 ? rate : 48000;
        m_ac = channels >= 2 ? 2 : 1;
    }

    bool open(int w, int h, int fps, const QString& path, QString* err) override
    {
        m_w = w;
        m_h = h;
        m_fps = fps;
        if (FAILED(MFStartup(MF_VERSION))) return fail(err, QCoreApplication::translate("QingYing", "Media Foundation 初始化失败"));
        m_mf = true;
        IMFAttributes* attrs = nullptr;
        MFCreateAttributes(&attrs, 1);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        HRESULT hr = MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, attrs, &m_writer);
        releaseCom(attrs);
        if (FAILED(hr)) return fail(err, QCoreApplication::translate("QingYing", "无法创建 MP4 文件（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
        IMFMediaType* out = nullptr;
        MFCreateMediaType(&out);
        out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        const qint64 rate = std::clamp<qint64>(qint64(w) * h * fps / 6, 1000000, 40000000);
        out->SetUINT32(MF_MT_AVG_BITRATE, UINT32(rate));
        m_rate = rate;
        out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(out, MF_MT_FRAME_SIZE, w, h);
        MFSetAttributeRatio(out, MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = m_writer->AddStream(out, &m_stream);
        releaseCom(out);
        if (FAILED(hr)) return fail(err, QCoreApplication::translate("QingYing", "H.264 编码器不支持该尺寸（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
        IMFMediaType* in = nullptr;
        MFCreateMediaType(&in);
        in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        in->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(w * 4)); // 负 stride = 底朝上位图
        MFSetAttributeSize(in, MF_MT_FRAME_SIZE, w, h);
        MFSetAttributeRatio(in, MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = m_writer->SetInputMediaType(m_stream, in, nullptr);
        releaseCom(in);
        if (FAILED(hr)) return fail(err, QCoreApplication::translate("QingYing", "设置视频输入格式失败（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));

        if (m_wantAudio) {
            IMFMediaType* aOut = nullptr;
            MFCreateMediaType(&aOut);
            aOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            aOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC);
            aOut->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, UINT32(m_ar));
            aOut->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, UINT32(m_ac));
            aOut->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
            aOut->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000); // ~128 kbps
            aOut->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
            aOut->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
            hr = m_writer->AddStream(aOut, &m_aStream);
            releaseCom(aOut);
            if (FAILED(hr)) {
                m_wantAudio = false; // 编码器不支持则退化为无声视频，不整段失败
            } else {
                IMFMediaType* aIn = nullptr;
                MFCreateMediaType(&aIn);
                aIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                aIn->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
                aIn->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, UINT32(m_ar));
                aIn->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, UINT32(m_ac));
                aIn->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                aIn->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, UINT32(m_ac * 2));
                aIn->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, UINT32(m_ar * m_ac * 2));
                aIn->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
                hr = m_writer->SetInputMediaType(m_aStream, aIn, nullptr);
                releaseCom(aIn);
                if (FAILED(hr)) m_wantAudio = false;
                else m_audio = true;
            }
        }

        if (FAILED(m_writer->BeginWriting())) return fail(err, QCoreApplication::translate("QingYing", "开始写入失败"));
        return true;
    }

    bool write(const uchar* bgra, int stride, qint64 ms) override
    {
        const DWORD size = DWORD(m_w * 4 * m_h);
        IMFMediaBuffer* buf = nullptr;
        if (FAILED(MFCreateMemoryBuffer(size, &buf))) return false;
        BYTE* dst = nullptr;
        buf->Lock(&dst, nullptr, nullptr);
        MFCopyImage(dst, m_w * 4, bgra, stride, m_w * 4, m_h);
        buf->Unlock();
        buf->SetCurrentLength(size);
        IMFSample* sample = nullptr;
        MFCreateSample(&sample);
        sample->AddBuffer(buf);
        const LONGLONG t = ms * 10'000;
        sample->SetSampleTime(t);
        sample->SetSampleDuration(qMax<LONGLONG>(1, 10'000'000LL / m_fps));
        const HRESULT hr = m_writer->WriteSample(m_stream, sample);
        releaseCom(sample);
        releaseCom(buf);
        return SUCCEEDED(hr);
    }

    bool writeAudio(const int16_t* pcm, uint32_t frames, int channels, int rate, qint64 ms) override
    {
        if (!m_audio || !pcm || frames == 0) return true;
        const DWORD bytes = DWORD(frames * channels * sizeof(int16_t));
        IMFMediaBuffer* buf = nullptr;
        if (FAILED(MFCreateMemoryBuffer(bytes, &buf))) return false;
        BYTE* dst = nullptr;
        buf->Lock(&dst, nullptr, nullptr);
        memcpy(dst, pcm, bytes);
        buf->Unlock();
        buf->SetCurrentLength(bytes);
        IMFSample* sample = nullptr;
        MFCreateSample(&sample);
        sample->AddBuffer(buf);
        sample->SetSampleTime(ms * 10'000);
        sample->SetSampleDuration(LONGLONG(frames) * 10'000'000LL / qMax(1, rate));
        const HRESULT hr = m_writer->WriteSample(m_aStream, sample);
        releaseCom(sample);
        releaseCom(buf);
        if (SUCCEEDED(hr)) m_audioBytes += bytes;
        return SUCCEEDED(hr);
    }

    qint64 bytes(qint64 ms) const override { return m_rate / 8 * ms / 1000 + m_audioBytes; }

    bool close(QString* err) override
    {
        bool ok = true;
        if (m_writer && FAILED(m_writer->Finalize())) ok = fail(err, QCoreApplication::translate("QingYing", "写入 MP4 收尾失败（录制时长可能太短）"));
        releaseCom(m_writer);
        if (m_mf) MFShutdown();
        m_mf = false;
        return ok;
    }

    ~Mp4Sink() override
    {
        releaseCom(m_writer);
        if (m_mf) MFShutdown();
    }

private:
    bool fail(QString* err, const QString& msg)
    {
        if (err) *err = msg;
        return false;
    }
    IMFSinkWriter* m_writer = nullptr;
    DWORD m_stream = 0;
    DWORD m_aStream = 0;
    int m_w = 0, m_h = 0, m_fps = 15;
    int m_ar = 48000, m_ac = 1;
    qint64 m_rate = 0;
    qint64 m_audioBytes = 0;
    bool m_mf = false;
    bool m_wantAudio = false;
    bool m_audio = false;
public:
    bool hasAudio() const override { return m_audio; }
};

// GIF：抓帧线程只做拷贝，调色板量化与 LZW 编码放在独立线程；连续相同的帧合并为一帧（延长显示时间）
class GifSink : public Sink {
public:
    explicit GifSink(bool dither) : m_dither(dither) {}
    ~GifSink() override { stopWorker(); }

    bool open(int w, int h, int fps, const QString& path, QString* err) override
    {
        m_w = w;
        m_h = h;
        m_fps = fps;
        m_final = path;
        // gif.h 只接受窄字符路径：先写到 ASCII 临时文件再改名
        m_tmp = QDir::temp().filePath(QStringLiteral("qingying_%1.gif").arg(QUuid::createUuid().toString(QUuid::Id128)));
        if (!GifBegin(&m_gif, QFile::encodeName(m_tmp).constData(), w, h, 100 / fps)) {
            if (err) *err = QCoreApplication::translate("QingYing", "无法创建 GIF 临时文件");
            return false;
        }
        m_open = true;
        m_worker = std::thread([this] { work(); });
        return true;
    }

    bool write(const uchar* bgra, int stride, qint64 ms) override
    {
        const QImage view(bgra, m_w, m_h, stride, QImage::Format_RGB32);
        if (!m_last.isNull() && sameAs(view)) return true; // 画面未变化：上一帧继续显示
        if (!m_last.isNull()) enqueue(std::move(m_last), ms - m_lastMs);
        m_last = view.copy();
        m_lastMs = ms;
        return true;
    }

    qint64 bytes(qint64) const override { return m_bytes; }

    bool close(QString* err) override
    {
        if (!m_last.isNull()) enqueue(std::move(m_last), 1000 / m_fps);
        m_last = QImage();
        stopWorker();
        if (m_open) GifEnd(&m_gif);
        m_open = false;
        QFile::remove(m_final);
        if (!QFile::rename(m_tmp, m_final) && !(QFile::copy(m_tmp, m_final) && QFile::remove(m_tmp))) {
            if (err) *err = QCoreApplication::translate("QingYing", "无法写入 %1").arg(m_final);
            return false;
        }
        return true;
    }

private:
    struct Item {
        QImage img;
        qint64 durationMs;
    };
    bool sameAs(const QImage& view) const
    {
        for (int y = 0; y < m_h; ++y)
            if (memcmp(view.constScanLine(y), m_last.constScanLine(y), size_t(m_w) * 4) != 0) return false;
        return true;
    }
    void enqueue(QImage img, qint64 durationMs)
    {
        std::unique_lock lock(m_mutex);
        // 编码积压时让抓帧线程等待（抓帧循环会自动丢帧以保持时间轴）
        m_cv.wait(lock, [this] { return m_queue.size() < 16 || m_quit; });
        m_queue.push_back({std::move(img), durationMs});
        m_cv.notify_all();
    }
    void work()
    {
        for (;;) {
            Item it;
            {
                std::unique_lock lock(m_mutex);
                m_cv.wait(lock, [this] { return !m_queue.empty() || m_quit; });
                if (m_queue.empty()) return;
                it = std::move(m_queue.front());
                m_queue.pop_front();
                m_cv.notify_all();
            }
            const QImage rgba = it.img.convertToFormat(QImage::Format_RGBA8888);
            const uint32_t delay = uint32_t(qMax<qint64>(2, (it.durationMs + 5) / 10)); // 1/100 秒
            encode(rgba.constBits(), delay, it.durationMs);
            if (m_gif.f) m_bytes = qint64(ftell(m_gif.f));
        }
    }
    // 自适应调色板：由当前帧与上次建表的样本共同生成（中位切分），约每 2 秒或画面大幅变化时重建，
    // 期间各帧共用同一调色板，避免逐帧调色板造成的闪烁与色带跳变；可选 Floyd–Steinberg 抖动
    void encode(const uchar* rgba, uint32_t delay, qint64 durationMs)
    {
        const size_t bytes = size_t(m_w) * m_h * 4;
        if (!m_mapper || m_sinceBuild >= 2000 || m_rebuild) {
            std::vector<uint32_t> samples;
            GifQuant::sample(rgba, m_w, m_h, 60000, samples);
            samples.insert(samples.end(), m_lastSamples.begin(), m_lastSamples.end());
            m_mapper = std::make_unique<GifQuant::Mapper>(GifQuant::medianCut(samples));
            m_lastSamples.clear();
            GifQuant::sample(rgba, m_w, m_h, 20000, m_lastSamples);
            m_sinceBuild = 0;
            m_rebuild = false;
        }
        m_out.resize(bytes);
        const int changed = GifQuant::quantize(rgba, m_prev.empty() ? nullptr : m_prev.data(), m_w, m_h, *m_mapper, m_dither, m_out.data());
        if (changed > m_w * m_h / 2) m_rebuild = true; // 场景切换：下一帧重建调色板
        m_sinceBuild += durationMs;
        GifPalette gp{};
        gp.bitDepth = 8;
        const auto& pal = m_mapper->palette();
        for (int i = 0; i < 256; ++i) {
            gp.r[i] = pal.rgb[i * 3];
            gp.g[i] = pal.rgb[i * 3 + 1];
            gp.b[i] = pal.rgb[i * 3 + 2];
        }
        GifWriteLzwImage(m_gif.f, m_out.data(), 0, 0, uint32_t(m_w), uint32_t(m_h), delay, &gp);
        m_prev.assign(rgba, rgba + bytes);
    }
    void stopWorker()
    {
        {
            std::lock_guard lock(m_mutex);
            m_quit = true;
        }
        m_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
    }
    GifWriter m_gif{};
    bool m_open = false;
    QImage m_last;
    qint64 m_lastMs = 0;
    QString m_tmp, m_final;
    int m_w = 0, m_h = 0, m_fps = 15;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Item> m_queue;
    bool m_quit = false;
    std::atomic<qint64> m_bytes{0};
    bool m_dither = false;
    std::unique_ptr<GifQuant::Mapper> m_mapper;
    std::vector<uint32_t> m_lastSamples;
    std::vector<uchar> m_prev, m_out;
    qint64 m_sinceBuild = 0;
    bool m_rebuild = false;
};

// Desktop Duplication：GPU 直接复制目标显示器画面，只回读选区，CPU 占用远低于 BitBlt。
// 不支持的情况（远程桌面、旋转屏、跨显示器选区、驱动不支持等）由调用方回退到 GDI。
class DxgiSource {
public:
    ~DxgiSource() { reset(); }

    bool init(const QRect& region, bool cursor)
    {
        m_drawCursor = cursor;
        m_region = region;
        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return false;
        IDXGIAdapter1* adapter = nullptr;
        for (UINT a = 0; !m_output && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
            IDXGIOutput* out = nullptr;
            for (UINT o = 0; adapter->EnumOutputs(o, &out) != DXGI_ERROR_NOT_FOUND; ++o) {
                DXGI_OUTPUT_DESC d{};
                out->GetDesc(&d);
                const QRect r(QPoint(d.DesktopCoordinates.left, d.DesktopCoordinates.top),
                              QPoint(d.DesktopCoordinates.right - 1, d.DesktopCoordinates.bottom - 1));
                if (d.AttachedToDesktop && d.Rotation <= DXGI_MODE_ROTATION_IDENTITY && r.contains(region)) {
                    out->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&m_output));
                    m_origin = r.topLeft();
                    releaseCom(out);
                    break;
                }
                releaseCom(out);
            }
            if (m_output) {
                D3D_FEATURE_LEVEL fl{};
                if (FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                             &m_device, &fl, &m_ctx)))
                    releaseCom(m_output);
            }
            releaseCom(adapter);
        }
        releaseCom(factory);
        if (!m_output || !m_device) {
            reset();
            return false;
        }
        m_box = {UINT(region.x() - m_origin.x()), UINT(region.y() - m_origin.y()), 0,
                 UINT(region.x() - m_origin.x() + region.width()), UINT(region.y() - m_origin.y() + region.height()), 1};
        D3D11_TEXTURE2D_DESC td{};
        td.Width = UINT(region.width());
        td.Height = UINT(region.height());
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_STAGING;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(m_device->CreateTexture2D(&td, nullptr, &m_staging)) || !duplicate()) {
            reset();
            return false;
        }
        // 第一帧：等到桌面画面到达，确保暂存纹理有内容
        for (int i = 0; i < 20 && !m_hasFrame; ++i)
            if (!acquire(50)) break;
        if (!m_hasFrame) {
            reset();
            return false;
        }
        return true;
    }

    // 取最新画面写入 dst（BGRA，自上而下）；画面没有更新时沿用上一帧。失败返回 false（调用方回退 GDI）。
    bool grab(uchar* dst, int stride, int w, int h)
    {
        if (!acquire(0)) return false;
        D3D11_MAPPED_SUBRESOURCE m{};
        if (FAILED(m_ctx->Map(m_staging, 0, D3D11_MAP_READ, 0, &m))) return false;
        for (int y = 0; y < h; ++y)
            memcpy(dst + size_t(y) * stride, static_cast<const uchar*>(m.pData) + size_t(y) * m.RowPitch, size_t(w) * 4);
        m_ctx->Unmap(m_staging, 0);
        if (m_drawCursor && m_ptrVisible && !m_shape.empty())
            CursorBlend::draw(dst, stride, w, h, m_shapeInfo.Type, m_shape.data(), int(m_shapeInfo.Width), int(m_shapeInfo.Height),
                              int(m_shapeInfo.Pitch), m_ptrPos.x() + m_origin.x() - m_region.x(), m_ptrPos.y() + m_origin.y() - m_region.y());
        return true;
    }

private:
    bool duplicate()
    {
        releaseCom(m_dup);
        return SUCCEEDED(m_output->DuplicateOutput(m_device, &m_dup));
    }
    bool acquire(UINT timeoutMs)
    {
        if (!m_dup && !duplicate()) return false;
        DXGI_OUTDUPL_FRAME_INFO info{};
        IDXGIResource* res = nullptr;
        HRESULT hr = m_dup->AcquireNextFrame(timeoutMs, &info, &res);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) return true;
        if (hr == DXGI_ERROR_ACCESS_LOST) { // 分辨率变化、UAC 安全桌面等：重建一次
            releaseCom(m_dup);
            return duplicate() || false;
        }
        if (FAILED(hr)) return false;
        // 指针：位置与形状随帧信息返回（物理像素，相对本显示器；Position 为形状左上角，已含热点偏移）
        if (info.LastMouseUpdateTime.QuadPart != 0) {
            m_ptrVisible = info.PointerPosition.Visible;
            m_ptrPos = QPoint(info.PointerPosition.Position.x, info.PointerPosition.Position.y);
        }
        if (info.PointerShapeBufferSize > 0) {
            std::vector<uchar> buf(info.PointerShapeBufferSize);
            UINT need = 0;
            DXGI_OUTDUPL_POINTER_SHAPE_INFO si{};
            if (SUCCEEDED(m_dup->GetFramePointerShape(UINT(buf.size()), buf.data(), &need, &si))) {
                m_shape = std::move(buf);
                m_shapeInfo = si;
            }
        }
        if (info.LastPresentTime.QuadPart != 0 || !m_hasFrame) {
            ID3D11Texture2D* tex = nullptr;
            if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex)))) {
                m_ctx->CopySubresourceRegion(m_staging, 0, 0, 0, 0, tex, 0, &m_box);
                m_hasFrame = true;
                releaseCom(tex);
            }
        }
        releaseCom(res);
        m_dup->ReleaseFrame();
        return true;
    }
    void reset()
    {
        releaseCom(m_dup);
        releaseCom(m_staging);
        releaseCom(m_ctx);
        releaseCom(m_device);
        releaseCom(m_output);
    }
    IDXGIOutput1* m_output = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;
    IDXGIOutputDuplication* m_dup = nullptr;
    ID3D11Texture2D* m_staging = nullptr;
    D3D11_BOX m_box{};
    QPoint m_origin;
    QRect m_region;
    bool m_hasFrame = false;
    bool m_drawCursor = false;
    bool m_ptrVisible = false;
    QPoint m_ptrPos;
    std::vector<uchar> m_shape;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO m_shapeInfo{};
};

void drawCursor(HDC dc, const QPoint& origin)
{
    CURSORINFO ci{};
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING)) return;
    ICONINFO ii{};
    if (!GetIconInfo(ci.hCursor, &ii)) return;
    DrawIconEx(dc, ci.ptScreenPos.x - int(ii.xHotspot) - origin.x(), ci.ptScreenPos.y - int(ii.yHotspot) - origin.y(),
               ci.hCursor, 0, 0, 0, nullptr, DI_NORMAL);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
}
}

Recorder::Recorder(const QRect& region, Format format, int fps, bool cursor, const QString& path)
    : m_region(region), m_format(format), m_fps(qBound(1, fps, 60)), m_cursor(cursor), m_path(path)
{
    m_region.setWidth(m_region.width() & ~1);
    m_region.setHeight(m_region.height() & ~1);
}

Recorder::~Recorder() { stop(); }

bool Recorder::start(QString* error)
{
    m_thread = std::thread([this] { run(); });
    while (m_started == 0) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (m_started < 0) {
        m_thread.join();
        if (error) *error = m_error;
        return false;
    }
    return true;
}

void Recorder::stop()
{
    m_stop = true;
    if (m_thread.joinable()) m_thread.join();
}

void Recorder::run()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const int w = m_region.width(), h = m_region.height();
    std::unique_ptr<Sink> sink;
    if (m_format == Format::Mp4) sink = std::make_unique<Mp4Sink>();
    else sink = std::make_unique<GifSink>(m_gifDither);

    std::unique_ptr<MicCapture> mic;
    m_micWarning.clear();
    m_micCapturing = false;
    if (m_mic && m_format == Format::Mp4) {
        mic = std::make_unique<MicCapture>();
        QString micErr;
        if (!mic->open(&micErr)) {
            m_micWarning = micErr;
            mic.reset();
        } else {
            mic->setMuted(m_micMuted);
            sink->enableAudio(mic->sampleRate(), mic->channels());
        }
    }

    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QString err;
    if (w < 16 || h < 16 || !sink->open(w, h, m_fps, m_path, &err)) {
        m_error = err.isEmpty() ? QCoreApplication::translate("QingYing", "录制区域太小") : err;
        if (mic) mic->stop();
        m_started = -1;
        CoUninitialize();
        return;
    }
    if (mic) {
        if (!sink->hasAudio()) {
            m_micWarning = QCoreApplication::translate("QingYing", "当前环境无法写入 AAC 音轨，已继续无声录制");
            mic->stop();
            mic.reset();
            m_micCapturing = false;
        } else {
            m_micCapturing = true;
        }
    }
    m_started = 1;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);

    auto dxgi = std::make_unique<DxgiSource>();
    if (qEnvironmentVariableIsSet("QINGYING_FORCE_GDI") || !dxgi->init(m_region, m_cursor)) dxgi.reset();
    m_backend = dxgi ? QStringLiteral("dxgi") : QStringLiteral("gdi");

    QElapsedTimer clock;
    clock.start();
    const qint64 interval = 1000 / m_fps;
    qint64 pausedTotal = 0, pauseStart = -1, next = 0;
    while (!m_stop) {
        if (m_paused) {
            if (pauseStart < 0) pauseStart = clock.elapsed();
            if (mic) {
                mic->setMuted(m_micMuted);
                std::vector<int16_t> discard;
                uint32_t df = 0;
                mic->drain(&discard, &df);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        if (pauseStart >= 0) {
            pausedTotal += clock.elapsed() - pauseStart;
            pauseStart = -1;
        }
        const qint64 t = clock.elapsed() - pausedTotal;
        if (t < next) {
            std::this_thread::sleep_for(std::chrono::milliseconds(qMin<qint64>(next - t, 20)));
            continue;
        }
        if (dxgi && !dxgi->grab(static_cast<uchar*>(bits), w * 4, w, h)) {
            dxgi.reset(); // Desktop Duplication 失效：本次录制余下部分改用 GDI
            m_backend = QStringLiteral("dxgi+gdi");
        }
        if (!dxgi) BitBlt(mem, 0, 0, w, h, screen, m_region.x(), m_region.y(), SRCCOPY);
        if (m_cursor && !dxgi) drawCursor(mem, m_region.topLeft()); // DXGI 模式下指针由帧信息绘制
        GdiFlush();
        if (!sink->write(static_cast<const uchar*>(bits), w * 4, t)) {
            m_error = QCoreApplication::translate("QingYing", "写入视频帧失败");
            break;
        }
        if (mic) {
            mic->setMuted(m_micMuted);
            if (!m_micMuted) {
                std::vector<int16_t> pcm;
                uint32_t aframes = 0;
                mic->drain(&pcm, &aframes);
                if (aframes > 0) {
                    const qint64 audioMs = qMax<qint64>(0, t - qint64(aframes) * 1000 / qMax(1, mic->sampleRate()));
                    if (!sink->writeAudio(pcm.data(), aframes, mic->channels(), mic->sampleRate(), audioMs)
                        && m_error.isEmpty()) {
                        m_micWarning = QCoreApplication::translate("QingYing", "麦克风写入失败，已继续无声录制");
                        m_micCapturing = false;
                        mic->stop();
                        mic.reset();
                    }
                }
            } else {
                std::vector<int16_t> discard;
                uint32_t df = 0;
                mic->drain(&discard, &df);
            }
        }
        ++m_frames;
        m_elapsed = t;
        m_bytes = sink->bytes(t);
        next += interval;
        if (t > next + interval * 3) next = t + interval; // 编码跟不上时丢帧
    }
    if (mic) {
        if (!m_micMuted) {
            std::vector<int16_t> pcm;
            uint32_t aframes = 0;
            mic->drain(&pcm, &aframes);
            if (aframes > 0)
                sink->writeAudio(pcm.data(), aframes, mic->channels(), mic->sampleRate(), m_elapsed.load());
        }
        mic->stop();
        mic.reset();
        m_micCapturing = false;
    }
    dxgi.reset();
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!sink->close(&err) && m_error.isEmpty()) m_error = err;
    sink.reset();
    CoUninitialize();
}
