#include "OcrPost.h"

#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
struct Pt {
    int x = 0;
    int y = 0;
};

int pyRoundDiv32(double value)
{
    const double scaled = value / 32.0;
    const double base = std::floor(scaled);
    const double frac = scaled - base;
    int whole = int(base);
    if (frac > 0.5) ++whole;
    else if (frac == 0.5 && (whole % 2) != 0) ++whole; // Python round：.5 向偶数
    return std::max(1, whole) * 32;
}

double cross(Pt o, Pt a, Pt b)
{
    return double(a.x - o.x) * (b.y - o.y) - double(a.y - o.y) * (b.x - o.x);
}

std::vector<Pt> convexHull(std::vector<Pt> pts)
{
    std::sort(pts.begin(), pts.end(), [](Pt a, Pt b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    pts.erase(std::unique(pts.begin(), pts.end(), [](Pt a, Pt b) { return a.x == b.x && a.y == b.y; }), pts.end());
    if (pts.size() <= 2) return pts;
    std::vector<Pt> hull;
    for (const Pt& p : pts) {
        while (hull.size() >= 2 && cross(hull[hull.size() - 2], hull.back(), p) <= 0) hull.pop_back();
        hull.push_back(p);
    }
    const size_t lower = hull.size();
    for (int i = int(pts.size()) - 2; i >= 0; --i) {
        while (hull.size() > lower && cross(hull[hull.size() - 2], hull.back(), pts[size_t(i)]) <= 0) hull.pop_back();
        hull.push_back(pts[size_t(i)]);
    }
    hull.pop_back();
    return hull;
}

OcrPost::Quad minAreaQuad(const std::vector<OcrPost::Vec2>& hull)
{
    OcrPost::Quad best;
    const int n = int(hull.size());
    if (n == 0) return best;
    if (n < 3) {
        float minx = hull[0].x, maxx = hull[0].x, miny = hull[0].y, maxy = hull[0].y;
        for (const auto& p : hull) {
            minx = std::min(minx, p.x);
            maxx = std::max(maxx, p.x);
            miny = std::min(miny, p.y);
            maxy = std::max(maxy, p.y);
        }
        best.p = {OcrPost::Vec2{minx, miny}, {maxx, miny}, {maxx, maxy}, {minx, maxy}};
        return best;
    }
    double bestArea = 1e100;
    for (int i = 0; i < n; ++i) {
        const auto a = hull[i];
        const auto b = hull[(i + 1) % n];
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) continue;
        const double c = dx / len, s = dy / len;
        double minx = 1e100, maxx = -1e100, miny = 1e100, maxy = -1e100;
        for (const auto& p : hull) {
            const double rx = p.x * c + p.y * s;
            const double ry = -p.x * s + p.y * c;
            minx = std::min(minx, rx);
            maxx = std::max(maxx, rx);
            miny = std::min(miny, ry);
            maxy = std::max(maxy, ry);
        }
        const double area = (maxx - minx) * (maxy - miny);
        if (area >= bestArea) continue;
        bestArea = area;
        auto inv = [&](double rx, double ry) {
            return OcrPost::Vec2{float(rx * c - ry * s), float(rx * s + ry * c)};
        };
        best.p = {inv(minx, miny), inv(maxx, miny), inv(maxx, maxy), inv(minx, maxy)};
    }
    return best;
}

OcrPost::Quad orderQuad(OcrPost::Quad q)
{
    int iTl = 0, iBr = 0;
    float minSum = 1e9f, maxSum = -1e9f;
    for (int i = 0; i < 4; ++i) {
        const float sum = q.p[size_t(i)].x + q.p[size_t(i)].y;
        if (sum < minSum) {
            minSum = sum;
            iTl = i;
        }
        if (sum > maxSum) {
            maxSum = sum;
            iBr = i;
        }
    }
    int rest[2], n = 0;
    for (int i = 0; i < 4; ++i)
        if (i != iTl && i != iBr) rest[n++] = i;
    if (n != 2) return q;
    const float d0 = q.p[size_t(rest[0])].y - q.p[size_t(rest[0])].x;
    const float d1 = q.p[size_t(rest[1])].y - q.p[size_t(rest[1])].x;
    const int iTr = d0 <= d1 ? rest[0] : rest[1];
    const int iBl = d0 <= d1 ? rest[1] : rest[0];
    return {q.p[size_t(iTl)], q.p[size_t(iTr)], q.p[size_t(iBr)], q.p[size_t(iBl)]};
}

float dist(OcrPost::Vec2 a, OcrPost::Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

double shoelace(const OcrPost::Quad& q)
{
    double a = 0;
    for (int i = 0; i < 4; ++i) {
        const auto p = q.p[size_t(i)];
        const auto n = q.p[size_t((i + 1) % 4)];
        a += double(p.x) * n.y - double(n.x) * p.y;
    }
    return a * 0.5;
}

OcrPost::Vec2 lineIntersect(OcrPost::Vec2 p1, OcrPost::Vec2 p2, OcrPost::Vec2 p3, OcrPost::Vec2 p4)
{
    const double d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    if (std::abs(d) < 1e-8) return p2;
    const double t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / d;
    return {float(p1.x + t * (p2.x - p1.x)), float(p1.y + t * (p2.y - p1.y))};
}

OcrPost::Quad unclip(OcrPost::Quad q, float ratio)
{
    OcrPost::Vec2 center;
    for (const auto& p : q.p) {
        center.x += p.x;
        center.y += p.y;
    }
    center.x *= 0.25f;
    center.y *= 0.25f;
    const double area = std::abs(shoelace(q));
    double peri = 0;
    for (int i = 0; i < 4; ++i) peri += dist(q.p[size_t(i)], q.p[size_t((i + 1) % 4)]);
    if (peri < 1e-3 || area < 1) return q;
    const double offset = area * ratio / peri;
    OcrPost::Vec2 a[4], b[4];
    for (int i = 0; i < 4; ++i) {
        const auto p = q.p[size_t(i)];
        const auto n = q.p[size_t((i + 1) % 4)];
        const double dx = n.x - p.x, dy = n.y - p.y;
        const double len = std::hypot(dx, dy);
        if (len < 1e-6) return q;
        OcrPost::Vec2 normal{float(-dy / len), float(dx / len)};
        const OcrPost::Vec2 mid{(p.x + n.x) * 0.5f, (p.y + n.y) * 0.5f};
        const float away = normal.x * (mid.x - center.x) + normal.y * (mid.y - center.y);
        if (away < 0) {
            normal.x = -normal.x;
            normal.y = -normal.y;
        }
        a[i] = {float(p.x + normal.x * offset), float(p.y + normal.y * offset)};
        b[i] = {float(n.x + normal.x * offset), float(n.y + normal.y * offset)};
    }
    OcrPost::Quad out;
    for (int i = 0; i < 4; ++i) {
        const int prev = (i + 3) % 4;
        out.p[size_t(i)] = lineIntersect(a[prev], b[prev], a[i], b[i]);
    }
    return out;
}

float shortSide(const OcrPost::Quad& q)
{
    return std::min(dist(q.p[0], q.p[1]), dist(q.p[0], q.p[3]));
}

bool pointInQuad(OcrPost::Vec2 p, const OcrPost::Quad& q)
{
    bool inside = false;
    for (int i = 0, j = 3; i < 4; j = i++) {
        const auto a = q.p[size_t(i)];
        const auto b = q.p[size_t(j)];
        const bool hit = ((a.y > p.y) != (b.y > p.y)) && (p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y + 1e-12f) + a.x);
        if (hit) inside = !inside;
    }
    return inside;
}

float boxScore(const float* prob, int w, int h, const OcrPost::Quad& q)
{
    float minx = q.p[0].x, maxx = q.p[0].x, miny = q.p[0].y, maxy = q.p[0].y;
    for (const auto& p : q.p) {
        minx = std::min(minx, p.x);
        maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y);
        maxy = std::max(maxy, p.y);
    }
    const int x0 = std::clamp(int(std::floor(minx)), 0, w - 1);
    const int x1 = std::clamp(int(std::ceil(maxx)), 0, w - 1);
    const int y0 = std::clamp(int(std::floor(miny)), 0, h - 1);
    const int y1 = std::clamp(int(std::ceil(maxy)), 0, h - 1);
    const int bw = std::max(1, x1 - x0);
    const int bh = std::max(1, y1 - y0);
    const int step = (bw * bh > 8000) ? 2 : 1;
    double sum = 0;
    int count = 0;
    for (int y = y0; y <= y1; y += step) {
        for (int x = x0; x <= x1; x += step) {
            if (!pointInQuad({float(x), float(y)}, q)) continue;
            const float v = prob[y * w + x];
            if (std::isfinite(v)) {
                sum += v;
                ++count;
            }
        }
    }
    return count ? float(sum / count) : 0;
}

bool solve8(double m[8][9])
{
    for (int col = 0; col < 8; ++col) {
        int pivot = col;
        for (int r = col + 1; r < 8; ++r)
            if (std::abs(m[r][col]) > std::abs(m[pivot][col])) pivot = r;
        if (std::abs(m[pivot][col]) < 1e-12) return false;
        if (pivot != col)
            for (int c = col; c < 9; ++c) std::swap(m[col][c], m[pivot][c]);
        const double div = m[col][col];
        for (int c = col; c < 9; ++c) m[col][c] /= div;
        for (int r = 0; r < 8; ++r) {
            if (r == col) continue;
            const double f = m[r][col];
            for (int c = col; c < 9; ++c) m[r][c] -= f * m[col][c];
        }
    }
    return true;
}

bool homography(const OcrPost::Vec2 from[4], const OcrPost::Vec2 to[4], double h[8])
{
    double m[8][9] = {};
    for (int i = 0; i < 4; ++i) {
        const double u = from[i].x, v = from[i].y, x = to[i].x, y = to[i].y;
        const int r = i * 2;
        m[r][0] = u;
        m[r][1] = v;
        m[r][2] = 1;
        m[r][6] = -u * x;
        m[r][7] = -v * x;
        m[r][8] = x;
        m[r + 1][3] = u;
        m[r + 1][4] = v;
        m[r + 1][5] = 1;
        m[r + 1][6] = -u * y;
        m[r + 1][7] = -v * y;
        m[r + 1][8] = y;
    }
    if (!solve8(m)) return false;
    for (int i = 0; i < 8; ++i) h[i] = m[i][8];
    return true;
}

void sample(const QImage& src, double sx, double sy, uchar* dst)
{
    const int w = src.width(), h = src.height();
    const int x0 = int(std::floor(sx)), y0 = int(std::floor(sy));
    const double fx = sx - x0, fy = sy - y0;
    auto pix = [&](int x, int y) {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return src.constScanLine(y) + x * 3;
    };
    const uchar* p00 = pix(x0, y0);
    const uchar* p10 = pix(x0 + 1, y0);
    const uchar* p01 = pix(x0, y0 + 1);
    const uchar* p11 = pix(x0 + 1, y0 + 1);
    for (int c = 0; c < 3; ++c) {
        const double top = p00[c] * (1 - fx) + p10[c] * fx;
        const double bot = p01[c] * (1 - fx) + p11[c] * fx;
        dst[c] = uchar(std::clamp(top * (1 - fy) + bot * fy, 0.0, 255.0));
    }
}

QImage rot90ccw(const QImage& src)
{
    QImage dst(src.height(), src.width(), QImage::Format_RGB888);
    const int w = src.width();
    for (int y = 0; y < dst.height(); ++y) {
        uchar* row = dst.scanLine(y);
        for (int x = 0; x < dst.width(); ++x) {
            const uchar* s = src.constScanLine(x) + (w - 1 - y) * 3;
            row[x * 3] = s[0];
            row[x * 3 + 1] = s[1];
            row[x * 3 + 2] = s[2];
        }
    }
    return dst;
}
} // namespace

namespace OcrPost {
DetSize detInputSize(int width, int height, int limit)
{
    DetSize s;
    if (width < 1 || height < 1) return s;
    double ratio = 1;
    const int longSide = std::max(width, height);
    if (limit > 0 && longSide > limit) ratio = double(limit) / longSide;
    s.width = std::max(32, pyRoundDiv32(width * ratio));
    s.height = std::max(32, pyRoundDiv32(height * ratio));
    return s;
}

QList<TextBox> detectBoxes(const float* prob, int width, int height, int origWidth, int origHeight, float thresh, float boxThresh,
                           float unclipRatio)
{
    QList<TextBox> boxes;
    if (!prob || width < 4 || height < 4 || origWidth < 1 || origHeight < 1) return boxes;
    std::vector<uint8_t> bin(size_t(width * height), 0);
    for (int i = 0; i < width * height; ++i)
        if (std::isfinite(prob[i]) && prob[i] > thresh) bin[size_t(i)] = 1;
    std::vector<uint8_t> dil(bin.size(), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t v = 0;
            for (int dy = 0; dy < 2 && !v; ++dy)
                for (int dx = 0; dx < 2; ++dx)
                    if (x + dx < width && y + dy < height && bin[size_t((y + dy) * width + x + dx)]) v = 1;
            dil[size_t(y * width + x)] = v;
        }
    }
    std::vector<uint8_t> seen(dil.size(), 0);
    const int dx8[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int dy8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    std::vector<int> stack;
    for (int start = 0; start < width * height; ++start) {
        if (!dil[size_t(start)] || seen[size_t(start)]) continue;
        stack.clear();
        stack.push_back(start);
        seen[size_t(start)] = 1;
        std::vector<Pt> boundary;
        int minx = width, miny = height, maxx = 0, maxy = 0, count = 0;
        while (!stack.empty()) {
            const int cur = stack.back();
            stack.pop_back();
            const int x = cur % width, y = cur / width;
            ++count;
            minx = std::min(minx, x);
            miny = std::min(miny, y);
            maxx = std::max(maxx, x);
            maxy = std::max(maxy, y);
            bool edge = false;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + dx8[k], ny = y + dy8[k];
                if (nx < 0 || ny < 0 || nx >= width || ny >= height || !dil[size_t(ny * width + nx)]) {
                    edge = true;
                    continue;
                }
                if (!seen[size_t(ny * width + nx)]) {
                    seen[size_t(ny * width + nx)] = 1;
                    stack.push_back(ny * width + nx);
                }
            }
            if (edge) boundary.push_back({x, y});
        }
        if (count < 8 || maxx - minx < 2 || maxy - miny < 2) continue;
        if (boundary.size() > 6000) {
            const size_t step = boundary.size() / 6000 + 1;
            std::vector<Pt> slim;
            for (size_t i = 0; i < boundary.size(); i += step) slim.push_back(boundary[i]);
            boundary.swap(slim);
        }
        std::vector<Vec2> hull;
        for (const Pt& p : convexHull(std::move(boundary))) hull.push_back({float(p.x), float(p.y)});
        Quad quad = orderQuad(minAreaQuad(hull));
        if (shortSide(quad) < 3.f) continue;
        const float score = boxScore(prob, width, height, quad);
        if (score < boxThresh) continue;
        quad = orderQuad(unclip(quad, unclipRatio));
        if (shortSide(quad) < 3.f) continue;
        const float sx = float(origWidth) / float(width);
        const float sy = float(origHeight) / float(height);
        for (auto& p : quad.p) {
            p.x = std::clamp(p.x * sx, 0.f, float(origWidth - 1));
            p.y = std::clamp(p.y * sy, 0.f, float(origHeight - 1));
        }
        boxes.push_back({quad, score});
        if (boxes.size() >= 1000) return boxes;
    }
    return boxes;
}

QImage warpQuad(const QImage& src, const Quad& quad)
{
    if (src.isNull()) return {};
    const QImage rgb = src.format() == QImage::Format_RGB888 ? src : src.convertToFormat(QImage::Format_RGB888);
    const int cropW = std::clamp(int(std::lround(std::max(dist(quad.p[0], quad.p[1]), dist(quad.p[2], quad.p[3])))), 1, 4000);
    const int cropH = std::clamp(int(std::lround(std::max(dist(quad.p[0], quad.p[3]), dist(quad.p[1], quad.p[2])))), 1, 4000);
    const Vec2 from[4] = {{0, 0}, {float(cropW), 0}, {float(cropW), float(cropH)}, {0, float(cropH)}};
    double h[8];
    if (!homography(from, quad.p.data(), h)) return {};
    QImage out(cropW, cropH, QImage::Format_RGB888);
    for (int y = 0; y < cropH; ++y) {
        uchar* row = out.scanLine(y);
        for (int x = 0; x < cropW; ++x) {
            const double u = x, v = y;
            double den = h[6] * u + h[7] * v + 1.0;
            if (std::abs(den) < 1e-8) den = den < 0 ? -1e-8 : 1e-8;
            sample(rgb, (h[0] * u + h[1] * v + h[2]) / den, (h[3] * u + h[4] * v + h[5]) / den, row + x * 3);
        }
    }
    if (cropH >= int(cropW * 1.5)) out = rot90ccw(out);
    return out;
}

QString ctcGreedy(const float* logits, int timeSteps, int classes, const QStringList& dict)
{
    if (!logits || timeSteps < 1 || classes < 2) return {};
    QString text;
    int last = -1;
    for (int t = 0; t < timeSteps; ++t) {
        const float* row = logits + size_t(t) * size_t(classes);
        int best = 0;
        float bestV = row[0];
        for (int c = 1; c < classes; ++c) {
            if (row[c] > bestV) {
                bestV = row[c];
                best = c;
            }
        }
        if (best != 0 && best != last) {
            const int idx = best - 1;
            if (idx >= 0 && idx < dict.size()) text += dict.at(idx);
        }
        last = best;
    }
    return text;
}

QStringList dictFromUtf8(const QByteArray& utf8)
{
    QByteArray data = utf8;
    if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);
    QStringList dict;
    int pos = 0;
    while (pos < data.size()) {
        int end = data.indexOf('\n', pos);
        if (end < 0) end = data.size();
        QByteArray line = data.mid(pos, end - pos);
        if (line.endsWith('\r')) line.chop(1);
        dict << QString::fromUtf8(line);
        pos = end + 1;
    }
    while (!dict.isEmpty() && dict.last().isEmpty()) dict.removeLast();
    return dict;
}

QString polishMixed(const QString& in)
{
    auto han = [](QChar c) { return c.script() == QChar::Script_Han; };
    auto latin = [](QChar c) { return c.isLetter() && c.script() == QChar::Script_Latin; };
    QString out;
    out.reserve(in.size() + 4);
    for (int i = 0; i < in.size(); ++i) {
        if (i > 0 && !in[i - 1].isSpace() && !in[i].isSpace()
            && ((han(in[i - 1]) && latin(in[i])) || (latin(in[i - 1]) && han(in[i]))))
            out += QLatin1Char(' ');
        out += in[i];
    }
    return out;
}

QString joinLines(QList<LinePiece> pieces)
{
    QList<LinePiece> items;
    for (auto& p : pieces) {
        if (p.text.isEmpty() || p.box.isEmpty()) continue;
        items.push_back(std::move(p));
    }
    std::sort(items.begin(), items.end(), [](const LinePiece& a, const LinePiece& b) {
        if (std::abs(a.box.top() - b.box.top()) > 2) return a.box.top() < b.box.top();
        return a.box.left() < b.box.left();
    });
    struct Line {
        double top = 0;
        double bottom = 0;
        QList<LinePiece> items;
    };
    QList<Line> lines;
    for (const auto& item : items) {
        int found = -1;
        for (int i = lines.size() - 1; i >= 0 && i >= lines.size() - 4; --i) {
            const double overlap = std::min(item.box.bottom(), lines[i].bottom) - std::max(item.box.top(), lines[i].top);
            const double h = std::min(item.box.height(), lines[i].bottom - lines[i].top);
            if (h > 1 && overlap > h * 0.45) {
                found = i;
                break;
            }
        }
        if (found < 0) {
            lines.push_back({item.box.top(), item.box.bottom(), {item}});
        } else {
            lines[found].top = std::min(lines[found].top, item.box.top());
            lines[found].bottom = std::max(lines[found].bottom, item.box.bottom());
            lines[found].items.push_back(item);
        }
    }
    QStringList out;
    for (auto& line : lines) {
        std::sort(line.items.begin(), line.items.end(), [](const LinePiece& a, const LinePiece& b) { return a.box.left() < b.box.left(); });
        QString text;
        for (int i = 0; i < line.items.size(); ++i) {
            if (i > 0) {
                const double gap = line.items[i].box.left() - line.items[i - 1].box.right();
                const double gapNeed = std::max(4.0, line.items[i - 1].box.height() * 0.35);
                if (gap > gapNeed) text += QLatin1Char(' ');
            }
            text += line.items[i].text;
        }
        text = polishMixed(text);
        if (!text.trimmed().isEmpty()) out << text;
    }
    return out.join(QLatin1Char('\n'));
}
}
