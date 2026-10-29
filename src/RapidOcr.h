#pragma once
#include "Ocr.h"

namespace RapidOcr {
bool runtimeAvailable();
// 阻塞。模型需已就位；失败时不回退系统 OCR（由 Ocr::recognize 决定）。
Ocr::Result recognize(const QImage& image);
}
