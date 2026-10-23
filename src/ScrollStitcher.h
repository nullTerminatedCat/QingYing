#pragma once
#include <QImage>
#include <QList>
#include <QSize>

// Pure image operations: no screen capture, input injection or UI dependencies.
namespace ScrollStitcher {
struct Match {
    int shift = -1;
    int fixedTop = 0;
    int fixedBottom = 0;
};

// Match only the scrolling body; identical edge rows are fixed chrome.
Match match(const QImage& previous, const QImage& current);

class Buffer {
public:
    enum class AppendResult { Added, InvalidFrame, LimitReached, AllocationFailed };
    explicit Buffer(int maxHeight = 30000, qint64 maxPixels = 64 * 1024 * 1024);
    bool reset(const QImage& first);
    AppendResult append(const QImage& frame, const Match& match);
    QSize size() const { return {m_width, m_height}; }
    int frameCount() const { return m_frames; }
    bool isEmpty() const { return m_parts.isEmpty(); }
    QImage image() const;
    QImage preview(const QSize& bounds) const;

private:
    QList<QImage> m_parts;
    int m_width = 0;
    int m_height = 0;
    int m_frames = 0;
    int m_maxHeight;
    qint64 m_maxPixels;
};
}
