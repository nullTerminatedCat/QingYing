#include "GifPalette.h"

#include <algorithm>
#include <climits>
#include <cstring>

namespace GifQuant {
namespace {
struct Box {
    int lo[3], hi[3]; // 5 bit 坐标闭区间
    uint64_t count = 0;
};
constexpr int B = 5, N = 1 << B;
inline int binOf(int r, int g, int b) { return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3); }

void shrink(Box& box, const std::vector<uint32_t>& hist)
{
    int lo[3] = {N, N, N}, hi[3] = {-1, -1, -1};
    uint64_t count = 0;
    for (int r = box.lo[0]; r <= box.hi[0]; ++r)
        for (int g = box.lo[1]; g <= box.hi[1]; ++g)
            for (int b = box.lo[2]; b <= box.hi[2]; ++b) {
                const uint32_t c = hist[(r << 10) | (g << 5) | b];
                if (!c) continue;
                count += c;
                const int v[3] = {r, g, b};
                for (int k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], v[k]);
                    hi[k] = std::max(hi[k], v[k]);
                }
            }
    box.count = count;
    if (count)
        for (int k = 0; k < 3; ++k) {
            box.lo[k] = lo[k];
            box.hi[k] = hi[k];
        }
}
}

Palette medianCut(const std::vector<uint32_t>& samples, int maxColors)
{
    Palette pal;
    maxColors = std::clamp(maxColors, 1, 255);
    if (samples.empty()) {
        pal.size = 2; // 至少一种颜色（黑）
        return pal;
    }
    // 少量不同颜色时直接精确使用
    {
        std::vector<uint32_t> uniq(samples);
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        if (int(uniq.size()) <= maxColors) {
            for (size_t i = 0; i < uniq.size(); ++i) {
                pal.rgb[(i + 1) * 3] = uint8_t(uniq[i] >> 16);
                pal.rgb[(i + 1) * 3 + 1] = uint8_t(uniq[i] >> 8);
                pal.rgb[(i + 1) * 3 + 2] = uint8_t(uniq[i]);
            }
            pal.size = int(uniq.size()) + 1;
            return pal;
        }
    }
    std::vector<uint32_t> hist(N * N * N, 0);
    std::vector<uint64_t> sum(N * N * N * 3, 0);
    for (uint32_t s : samples) {
        const int r = (s >> 16) & 255, g = (s >> 8) & 255, b = s & 255;
        const int i = binOf(r, g, b);
        ++hist[i];
        sum[i * 3] += r;
        sum[i * 3 + 1] += g;
        sum[i * 3 + 2] += b;
    }
    std::vector<Box> boxes(1);
    boxes[0] = {{0, 0, 0}, {N - 1, N - 1, N - 1}, 0};
    shrink(boxes[0], hist);
    while (int(boxes.size()) < maxColors) {
        // 选择「像素数 × 最长边」最大的盒子切分：既照顾常见颜色，也照顾跨度大的渐变
        int best = -1;
        uint64_t bestScore = 0;
        for (int i = 0; i < int(boxes.size()); ++i) {
            const Box& b = boxes[i];
            const int len = std::max({b.hi[0] - b.lo[0], b.hi[1] - b.lo[1], b.hi[2] - b.lo[2]});
            if (len == 0) continue;
            const uint64_t score = b.count * uint64_t(len);
            if (score > bestScore) {
                bestScore = score;
                best = i;
            }
        }
        if (best < 0) break;
        Box box = boxes[best];
        int axis = 0;
        for (int k = 1; k < 3; ++k)
            if (box.hi[k] - box.lo[k] > box.hi[axis] - box.lo[axis]) axis = k;
        // 沿该轴按像素数取中位
        std::vector<uint64_t> plane(N, 0);
        for (int r = box.lo[0]; r <= box.hi[0]; ++r)
            for (int g = box.lo[1]; g <= box.hi[1]; ++g)
                for (int b = box.lo[2]; b <= box.hi[2]; ++b) {
                    const int v[3] = {r, g, b};
                    plane[v[axis]] += hist[(r << 10) | (g << 5) | b];
                }
        uint64_t acc = 0;
        int cut = box.lo[axis];
        for (int v = box.lo[axis]; v < box.hi[axis]; ++v) {
            acc += plane[v];
            cut = v;
            if (acc * 2 >= box.count) break;
        }
        Box a = box, c = box;
        a.hi[axis] = cut;
        c.lo[axis] = cut + 1;
        shrink(a, hist);
        shrink(c, hist);
        boxes[best] = a;
        if (c.count) boxes.push_back(c);
        if (!a.count) boxes.erase(boxes.begin() + best);
    }
    int idx = 1;
    for (const Box& b : boxes) {
        uint64_t s[3] = {0, 0, 0}, n = 0;
        for (int r = b.lo[0]; r <= b.hi[0]; ++r)
            for (int g = b.lo[1]; g <= b.hi[1]; ++g)
                for (int bb = b.lo[2]; bb <= b.hi[2]; ++bb) {
                    const int i = (r << 10) | (g << 5) | bb;
                    n += hist[i];
                    s[0] += sum[i * 3];
                    s[1] += sum[i * 3 + 1];
                    s[2] += sum[i * 3 + 2];
                }
        if (!n || idx > 255) continue;
        for (int k = 0; k < 3; ++k) pal.rgb[idx * 3 + k] = uint8_t((s[k] + n / 2) / n);
        ++idx;
    }
    pal.size = idx;
    return pal;
}

void sample(const uint8_t* rgba, int w, int h, int maxSamples, std::vector<uint32_t>& out)
{
    const int64_t total = int64_t(w) * h;
    if (total <= 0 || maxSamples <= 0) return;
    const int64_t step = std::max<int64_t>(1, total / maxSamples);
    for (int64_t i = 0; i < total; i += step) {
        const uint8_t* p = rgba + i * 4;
        out.push_back((uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2]);
    }
}

Mapper::Mapper(const Palette& p) : m_pal(p), m_cache(64 * 64 * 64, 0) {}

int Mapper::nearest(int r, int g, int b)
{
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    const int key = ((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2);
    if (m_cache[key]) return m_cache[key];
    // 用桶中心查找，保证同一桶结果一致
    const int cr = (r & ~3) + 2, cg = (g & ~3) + 2, cb = (b & ~3) + 2;
    int best = 1, bestD = INT_MAX;
    for (int i = 1; i < m_pal.size; ++i) {
        const int dr = cr - m_pal.rgb[i * 3], dg = cg - m_pal.rgb[i * 3 + 1], db = cb - m_pal.rgb[i * 3 + 2];
        const int d = 2 * dr * dr + 4 * dg * dg + 3 * db * db; // 近似感知加权
        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    m_cache[key] = uint8_t(best);
    return best;
}

int quantize(const uint8_t* cur, const uint8_t* prev, int w, int h, Mapper& mapper, bool dither, uint8_t* out)
{
    const Palette& pal = mapper.palette();
    std::vector<int> err(dither ? size_t(w + 2) * 2 * 3 : 0, 0); // 两行误差（×16）
    int changed = 0;
    for (int y = 0; y < h; ++y) {
        int* errCur = dither ? &err[size_t((y & 1)) * (w + 2) * 3] : nullptr;
        int* errNext = dither ? &err[size_t(((y + 1) & 1)) * (w + 2) * 3] : nullptr;
        if (dither) std::fill(errNext, errNext + (w + 2) * 3, 0);
        for (int x = 0; x < w; ++x) {
            const size_t i = (size_t(y) * w + x) * 4;
            const uint8_t* p = cur + i;
            uint8_t* o = out + i;
            if (prev && memcmp(p, prev + i, 3) == 0) { // 与上一帧相同：透明，沿用上一帧显示
                o[3] = uint8_t(kTransparent);
                continue;
            }
            ++changed;
            int r = p[0], g = p[1], b = p[2];
            if (dither) {
                const int* e = errCur + (x + 1) * 3;
                r += e[0] / 16;
                g += e[1] / 16;
                b += e[2] / 16;
            }
            const int idx = mapper.nearest(r, g, b);
            o[0] = pal.rgb[idx * 3];
            o[1] = pal.rgb[idx * 3 + 1];
            o[2] = pal.rgb[idx * 3 + 2];
            o[3] = uint8_t(idx);
            if (dither) {
                const int er = std::clamp(r, 0, 255) - o[0], eg = std::clamp(g, 0, 255) - o[1], eb = std::clamp(b, 0, 255) - o[2];
                const int ev[3] = {er, eg, eb};
                for (int k = 0; k < 3; ++k) {
                    errCur[(x + 2) * 3 + k] += ev[k] * 7;
                    errNext[(x) * 3 + k] += ev[k] * 3;
                    errNext[(x + 1) * 3 + k] += ev[k] * 5;
                    errNext[(x + 2) * 3 + k] += ev[k] * 1;
                }
            }
        }
    }
    return changed;
}
}
