#pragma once
#include <cstdint>

// 把 Desktop Duplication 返回的指针形状合成到 BGRA 画面上（纯函数，便于单元测试）
namespace CursorBlend {
// 与 DXGI_OUTDUPL_POINTER_SHAPE_TYPE 取值一致
enum Type : unsigned { Monochrome = 1, Color = 2, MaskedColor = 4 };
// dst：BGRA，自上而下；(x, y)：形状左上角在 dst 中的位置，可以部分或全部在外面。
// Monochrome 形状高度为 AND + XOR 两个 1bpp 掩码之和（height 为总高度）。
void draw(unsigned char* dst, int dstStride, int dstW, int dstH, unsigned type, const unsigned char* shape, int shapeW,
          int shapeH, int shapePitch, int x, int y);
}
