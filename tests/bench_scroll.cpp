#include "../src/ScrollStitcher.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QPainter>
#include <cstdio>

// Reproducible comparison of the v0.5.2 full-image-copy loop and the strip buffer.
// Matching and screen capture are excluded from both sides. No pass/fail timing threshold.
int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    constexpr int w = 1280, h = 720, shift = 120, frames = 80;
    QImage frame(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) frame.setPixel(x, y, qRgb(x % 251, y % 253, (x ^ y) & 255));
    QImage baseline = frame;
    QElapsedTimer timer;
    timer.start();
    for (int i = 1; i < frames; ++i) {
        QImage merged(w, baseline.height() + shift, QImage::Format_RGB32);
        {
            QPainter p(&merged);
            p.drawImage(0, 0, baseline);
            p.drawImage(QPoint(0, baseline.height()), frame, QRect(0, h - shift, w, shift));
        }
        baseline = merged;
        const QImage scaled = baseline.scaledToWidth(180, Qt::SmoothTransformation);
        const QImage preview = scaled.copy(0, qMax(0, scaled.height() - 320), 180, qMin(320, scaled.height()));
        if (preview.isNull()) return 1;
    }
    const qint64 baselineNs = timer.nsecsElapsed();
    ScrollStitcher::Buffer buffer;
    if (!buffer.reset(frame)) return 2;
    timer.restart();
    for (int i = 1; i < frames; ++i) {
        if (buffer.append(frame, {shift, 0, 0}) != ScrollStitcher::Buffer::AppendResult::Added) return 3;
        if (buffer.preview(QSize(180, 320)).isNull()) return 4;
    }
    const QImage result = buffer.image(); // Include final assembly in the new implementation's time.
    const qint64 stripNs = timer.nsecsElapsed();
    if (baseline != result) return 5;
    std::puts(qPrintable(QStringLiteral("frames=%1 image=%2x%3 baseline_ms=%4 strips_ms=%5 speedup=%6x pixels_equal=true")
                            .arg(frames).arg(w).arg(result.height())
                            .arg(baselineNs / 1e6, 0, 'f', 2).arg(stripNs / 1e6, 0, 'f', 2)
                            .arg(double(baselineNs) / qMax(qint64(1), stripNs), 0, 'f', 2)));
    return 0;
}
