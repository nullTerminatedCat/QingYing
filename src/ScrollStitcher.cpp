#include "ScrollStitcher.h"

#include <QPainter>
#include <cmath>

namespace ScrollStitcher {
Match match(const QImage& a, const QImage& b)
{
    if (a.isNull() || b.isNull() || a.size() != b.size() || a.height() < 20) return {};
    const QImage previous = a.convertToFormat(QImage::Format_RGB32);
    const QImage current = b.convertToFormat(QImage::Format_RGB32);
    if (previous.isNull() || current.isNull()) return {};
    const int w = a.width(), h = a.height();
    const int x0 = w > 120 ? 8 : 0;
    const int x1 = w > 120 ? w - 24 : w; // Ignore the scrollbar.
    QList<quint64> hp(h), hc(h);
    QList<bool> up(h), uc(h);
    auto rows = [&](const QImage& img, QList<quint64>& hashes, QList<bool>& uniform) {
        for (int y = 0; y < h; ++y) {
            const auto* row = reinterpret_cast<const QRgb*>(img.constScanLine(y));
            quint64 hash = 1469598103934665603ULL;
            bool flat = true;
            for (int x = x0; x < x1; ++x) {
                hash = (hash ^ (row[x] & 0xFFFFFF)) * 1099511628211ULL;
                if (row[x] != row[x0]) flat = false;
            }
            hashes[y] = hash;
            uniform[y] = flat;
        }
    };
    rows(previous, hp, up);
    rows(current, hc, uc);

    int informative = 0, unchanged = 0;
    for (int y = 0; y < h; ++y) {
        if (up[y] && uc[y]) continue;
        ++informative;
        if (hp[y] == hc[y]) ++unchanged;
    }
    if (hp == hc) return {informative >= 8 ? 0 : -1, 0, 0};

    int top = 0, bottom = 0;
    while (top < h && hp[top] == hc[top]) ++top;
    while (bottom < h - top && hp[h - 1 - bottom] == hc[h - 1 - bottom]) ++bottom;
    const int bodyHeight = h - top - bottom;
    int best = -1;
    double bestRatio = 0;
    for (int dy = 1; dy <= bodyHeight - qMax(8, bodyHeight / 5); ++dy) {
        int total = 0, matches = 0;
        for (int y = top; y + dy < h - bottom; ++y) {
            if (up[y + dy] && uc[y]) continue;
            ++total;
            if (hp[y + dy] == hc[y]) ++matches;
        }
        if (total < 8) continue;
        const double ratio = double(matches) / total;
        if (ratio > bestRatio + 1e-9) {
            bestRatio = ratio;
            best = dy;
        }
    }
    if (bestRatio >= 0.85) return {best, top, bottom};
    // Permit a small blinking caret at the bottom of an otherwise stationary page.
    if (informative >= 8 && double(unchanged) / informative >= 0.98) return {0, 0, 0};
    return {};
}

Buffer::Buffer(int maxHeight, qint64 maxPixels) : m_maxHeight(maxHeight), m_maxPixels(maxPixels) {}

bool Buffer::reset(const QImage& first)
{
    if (first.isNull() || first.height() > m_maxHeight
        || qint64(first.width()) * first.height() > m_maxPixels) return false;
    QImage image = first.convertToFormat(QImage::Format_RGB32);
    if (image.isNull()) return false;
    image.setDevicePixelRatio(1);
    m_parts = {image};
    m_width = image.width();
    m_height = image.height();
    m_frames = 1;
    return true;
}

Buffer::AppendResult Buffer::append(const QImage& frame, const Match& m)
{
    if (isEmpty() || frame.isNull() || frame.width() != m_width || m.shift <= 0
        || m.fixedTop < 0 || m.fixedBottom < 0 || m.fixedBottom > m_height
        || qint64(m.shift) + m.fixedTop + m.fixedBottom > frame.height()) return AppendResult::InvalidFrame;
    const qint64 nextHeight = qint64(m_height) + m.shift;
    if (nextHeight > m_maxHeight || nextHeight * m_width > m_maxPixels) return AppendResult::LimitReached;

    // Store only new pixels and the current footer. Never copy the growing image on every tick.
    QImage strip = frame.copy(0, frame.height() - m.fixedBottom - m.shift,
                             m_width, m.shift + m.fixedBottom).convertToFormat(QImage::Format_RGB32);
    if (strip.isNull()) return AppendResult::AllocationFailed;
    strip.setDevicePixelRatio(1);

    int remove = m.fixedBottom;
    int retained = int(m_parts.size());
    while (remove > 0 && retained > 0 && remove >= m_parts[retained - 1].height()) {
        remove -= m_parts[--retained].height();
    }
    QImage shortened;
    if (remove > 0) {
        const QImage& last = m_parts[retained - 1];
        shortened = last.copy(0, 0, m_width, last.height() - remove);
        if (shortened.isNull()) return AppendResult::AllocationFailed;
    }
    // Commit only after all image allocations succeed, preserving the result on failure.
    m_parts.resize(retained);
    if (remove > 0) m_parts.last() = shortened;
    m_parts.append(strip);
    m_height = int(nextHeight);
    ++m_frames;
    return AppendResult::Added;
}

QImage Buffer::image() const
{
    if (isEmpty()) return {};
    QImage out(m_width, m_height, QImage::Format_RGB32);
    if (out.isNull()) return {};
    QPainter painter(&out);
    int y = 0;
    for (const QImage& part : m_parts) {
        painter.drawImage(0, y, part);
        y += part.height();
    }
    return out;
}

QImage Buffer::preview(const QSize& bounds) const
{
    if (isEmpty() || bounds.isEmpty()) return {};
    const qreal scale = qreal(bounds.width()) / m_width;
    const int height = qMin(bounds.height(), qMax(1, int(std::ceil(m_height * scale))));
    QImage out(bounds.width(), height, QImage::Format_RGB32);
    if (out.isNull()) return {};
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    qreal bottom = height;
    for (auto it = m_parts.crbegin(); it != m_parts.crend() && bottom > 0; ++it) {
        const qreal partHeight = it->height() * scale;
        painter.drawImage(QRectF(0, bottom - partHeight, bounds.width(), partHeight), *it);
        bottom -= partHeight;
    }
    return out;
}
}
