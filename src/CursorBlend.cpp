#include "CursorBlend.h"

namespace CursorBlend {
void draw(unsigned char* dst, int stride, int dw, int dh, unsigned type, const unsigned char* shape, int sw, int sh, int pitch, int x0, int y0)
{
    if (!dst || !shape || sw <= 0 || sh <= 0) return;
    const int h = type == Monochrome ? sh / 2 : sh;
    for (int sy = 0; sy < h; ++sy) {
        const int y = y0 + sy;
        if (y < 0 || y >= dh) continue;
        unsigned char* row = dst + y * stride;
        for (int sx = 0; sx < sw; ++sx) {
            const int x = x0 + sx;
            if (x < 0 || x >= dw) continue;
            unsigned char* px = row + x * 4;
            if (type == Monochrome) {
                const int bit = 0x80 >> (sx % 8);
                const bool andBit = shape[sy * pitch + sx / 8] & bit;
                const bool xorBit = shape[(sy + h) * pitch + sx / 8] & bit;
                for (int c = 0; c < 3; ++c) {
                    unsigned char v = andBit ? px[c] : 0;
                    if (xorBit) v ^= 0xFF;
                    px[c] = v;
                }
            } else {
                const unsigned char* sp = shape + sy * pitch + sx * 4;
                if (type == Color) {
                    const unsigned a = sp[3];
                    for (int c = 0; c < 3; ++c) px[c] = (unsigned char)((sp[c] * a + px[c] * (255 - a) + 127) / 255);
                } else if (type == MaskedColor) {
                    if (sp[3] == 0) {
                        for (int c = 0; c < 3; ++c) px[c] = sp[c];
                    } else {
                        for (int c = 0; c < 3; ++c) px[c] ^= sp[c];
                    }
                }
            }
        }
    }
}
}
