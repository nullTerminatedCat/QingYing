#pragma once
#include <array>
#include <cstdint>
#include <vector>

// GIF 调色板量化：中位切分（median cut）生成全局 / 自适应调色板，最近色查找带缓存，
// 可选 Floyd–Steinberg 抖动；与上一帧相同的像素输出透明索引 0（帧间差分）。纯逻辑，见 tests/。
namespace GifQuant {
constexpr int kTransparent = 0; // 索引 0 保留为透明

struct Palette {
    int size = 1;                         // 含透明索引 0
    std::array<uint8_t, 256 * 3> rgb{};   // 索引 i 的颜色为 rgb[3i..3i+2]
};

// 从 RGB 样本（0xRRGGBB）生成至多 maxColors（≤255）种颜色，写入索引 1..
Palette medianCut(const std::vector<uint32_t>& samples, int maxColors = 255);
// 从 RGBA8888 图像均匀采样至多 maxSamples 个像素，追加到 out
void sample(const uint8_t* rgba, int w, int h, int maxSamples, std::vector<uint32_t>& out);

class Mapper {
public:
    explicit Mapper(const Palette& p);
    int nearest(int r, int g, int b); // 返回 1..size-1
    const Palette& palette() const { return m_pal; }

private:
    Palette m_pal;
    std::vector<uint8_t> m_cache; // 6 bit/通道，0 表示未计算
};

// 量化一帧。cur：RGBA8888；prevSrc：上一帧原图（可为空=首帧）；out：RGBA，A 通道为调色板索引（gif.h 格式）。
// 返回变化像素数。
int quantize(const uint8_t* cur, const uint8_t* prevSrc, int w, int h, Mapper& mapper, bool dither, uint8_t* out);
}
