#define _CRT_SECURE_NO_WARNINGS
#include "Recorder.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUuid>
#include <algorithm>
#include <chrono>

#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

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
};

class Mp4Sink : public Sink {
public:
    bool open(int w, int h, int fps, const QString& path, QString* err) override
    {
        m_w = w;
        m_h = h;
        m_fps = fps;
        if (FAILED(MFStartup(MF_VERSION))) return fail(err, QStringLiteral("Media Foundation 初始化失败"));
        m_mf = true;
        IMFAttributes* attrs = nullptr;
        MFCreateAttributes(&attrs, 1);
        attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
        HRESULT hr = MFCreateSinkWriterFromURL(reinterpret_cast<LPCWSTR>(path.utf16()), nullptr, attrs, &m_writer);
        releaseCom(attrs);
        if (FAILED(hr)) return fail(err, QStringLiteral("无法创建 MP4 文件（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
        IMFMediaType* out = nullptr;
        MFCreateMediaType(&out);
        out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        const qint64 rate = std::clamp<qint64>(qint64(w) * h * fps / 6, 1000000, 40000000);
        out->SetUINT32(MF_MT_AVG_BITRATE, UINT32(rate));
        out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        MFSetAttributeSize(out, MF_MT_FRAME_SIZE, w, h);
        MFSetAttributeRatio(out, MF_MT_FRAME_RATE, fps, 1);
        MFSetAttributeRatio(out, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        hr = m_writer->AddStream(out, &m_stream);
        releaseCom(out);
        if (FAILED(hr)) return fail(err, QStringLiteral("H.264 编码器不支持该尺寸（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
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
        if (FAILED(hr)) return fail(err, QStringLiteral("设置输入格式失败（0x%1）").arg(quint32(hr), 8, 16, QLatin1Char('0')));
        if (FAILED(m_writer->BeginWriting())) return fail(err, QStringLiteral("开始写入失败"));
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

    bool close(QString* err) override
    {
        bool ok = true;
        if (m_writer && FAILED(m_writer->Finalize())) ok = fail(err, QStringLiteral("写入 MP4 结尾失败（录制时间可能太短）"));
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
    bool m_mf = false;
};

class GifSink : public Sink {
public:
    bool open(int w, int h, int fps, const QString& path, QString* err) override
    {
        m_w = w;
        m_h = h;
        m_fps = fps;
        m_final = path;
        // gif.h 只接受窄字符路径：先写到 ASCII 临时文件再改名
        m_tmp = QDir::temp().filePath(QStringLiteral("qingying_%1.gif").arg(QUuid::createUuid().toString(QUuid::Id128)));
        if (!GifBegin(&m_gif, QFile::encodeName(m_tmp).constData(), w, h, 100 / fps)) {
            if (err) *err = QStringLiteral("无法创建 GIF 临时文件");
            return false;
        }
        return true;
    }

    bool write(const uchar* bgra, int stride, qint64 ms) override
    {
        if (!m_pending.isNull()) flush(ms - m_pendingMs);
        const QImage view(bgra, m_w, m_h, stride, QImage::Format_RGB32);
        m_pending = view.convertToFormat(QImage::Format_RGBA8888);
        m_pendingMs = ms;
        return true;
    }

    bool close(QString* err) override
    {
        if (!m_pending.isNull()) flush(1000 / m_fps);
        GifEnd(&m_gif);
        QFile::remove(m_final);
        if (!QFile::rename(m_tmp, m_final) && !(QFile::copy(m_tmp, m_final) && QFile::remove(m_tmp))) {
            if (err) *err = QStringLiteral("无法写入 %1").arg(m_final);
            return false;
        }
        return true;
    }

private:
    void flush(qint64 durationMs)
    {
        const uint32_t delay = uint32_t(qMax<qint64>(2, (durationMs + 5) / 10)); // 1/100 秒
        GifWriteFrame(&m_gif, m_pending.constBits(), m_w, m_h, delay);
        m_pending = QImage();
    }
    GifWriter m_gif{};
    QImage m_pending;
    qint64 m_pendingMs = 0;
    QString m_tmp, m_final;
    int m_w = 0, m_h = 0, m_fps = 15;
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
        m_error = err.isEmpty() ? QStringLiteral("录制区域太小") : err;
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
        BitBlt(mem, 0, 0, w, h, screen, m_region.x(), m_region.y(), SRCCOPY);
        if (m_cursor) drawCursor(mem, m_region.topLeft());
        GdiFlush();
        if (!sink->write(static_cast<const uchar*>(bits), w * 4, t)) {
            m_error = QStringLiteral("写入视频帧失败");
            break;
        }
        ++m_frames;
        m_elapsed = t;
        next += interval;
        if (t > next + interval * 3) next = t + interval; // 编码跟不上时丢帧
    }
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!sink->close(&err) && m_error.isEmpty()) m_error = err;
    sink.reset();
    CoUninitialize();
}
