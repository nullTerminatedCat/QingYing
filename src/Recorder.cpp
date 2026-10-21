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
    virtual bool close(QString* err) = 0;
    virtual qint64 bytes(qint64 ms) const = 0; // 当前文件大小估计
};

class Mp4Sink : public Sink {
public:
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
        in->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(w * 4)); // 正 stride = 自上而下
        MFSetAttributeSize(in, MF_MT_FRAME_SIZE, w, h);
        MFSetAttributeRatio(in, MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(in, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = m_writer->SetInputMediaType(m_stream, in, nullptr);
        releaseCom(in);
        if (FAILED(hr)) return fail(err, QCoreApplication::translate("QingYing", "设置输入格式失败（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
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

    qint64 bytes(qint64 ms) const override { return m_rate / 8 * ms / 1000; }

    bool close(QString* err) override
    {
        bool ok = true;
        if (m_writer && FAILED(m_writer->Finalize())) ok = fail(err, QCoreApplication::translate("QingYing", "写入 MP4 结尾失败（录制时间可能太短）"));
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
    int m_w = 0, m_h = 0, m_fps = 15;
    qint64 m_rate = 0;
    bool m_mf = false;
};

// GIF：抓帧线程只做拷贝，调色板量化与 LZW 编码放在独立线程；连续相同的帧合并为一帧（延长显示时间）
class GifSink : public Sink {
public:
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
            GifWriteFrame(&m_gif, rgba.constBits(), m_w, m_h, delay);
            if (m_gif.f) m_bytes = qint64(ftell(m_gif.f));
        }
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
};

// Desktop Duplication：GPU 直接复制目标显示器画面，只回读选区，CPU 占用远低于 BitBlt。
// 不支持的情况（远程桌面、旋转屏、跨显示器选区、驱动不支持等）由调用方回退到 GDI。
class DxgiSource {
public:
    ~DxgiSource() { reset(); }

    bool init(const QRect& region)
    {
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
    bool m_hasFrame = false;
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
    else sink = std::make_unique<GifSink>();
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QString err;
    if (w < 16 || h < 16 || !sink->open(w, h, m_fps, m_path, &err)) {
        m_error = err.isEmpty() ? QCoreApplication::translate("QingYing", "录制区域太小") : err;
        m_started = -1;
        CoUninitialize();
        return;
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
    if (qEnvironmentVariableIsSet("QINGYING_FORCE_GDI") || !dxgi->init(m_region)) dxgi.reset();
    m_backend = dxgi ? QStringLiteral("dxgi") : QStringLiteral("gdi");

    QElapsedTimer clock;
    clock.start();
    const qint64 interval = 1000 / m_fps;
    qint64 pausedTotal = 0, pauseStart = -1, next = 0;
    while (!m_stop) {
        if (m_paused) {
            if (pauseStart < 0) pauseStart = clock.elapsed();
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
        if (m_cursor) drawCursor(mem, m_region.topLeft());
        GdiFlush();
        if (!sink->write(static_cast<const uchar*>(bits), w * 4, t)) {
            m_error = QCoreApplication::translate("QingYing", "写入视频帧失败");
            break;
        }
        ++m_frames;
        m_elapsed = t;
        m_bytes = sink->bytes(t);
        next += interval;
        if (t > next + interval * 3) next = t + interval; // 编码跟不上时丢帧
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
