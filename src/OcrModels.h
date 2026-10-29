#pragma once
#include <QString>
#include <functional>

// RapidOCR（PP-OCRv4 mobile）模型：检测 / 识别 / 方向 / 字典。
// 查找顺序：环境变量 QINGYING_OCR_MODELS、exe 旁 models、本地 AppData。
namespace OcrModels {
struct Paths {
    QString directory;
    QString det;
    QString rec;
    QString cls;
    QString dict;
    bool ready = false; // 检测、识别、字典齐全且校验通过
};

Paths locate();
QString preferredDirectory();
// 阻塞下载缺失文件。progress 在调用线程触发，可为空。
bool ensure(QString* error, const std::function<void(const QString&)>& progress = {});
}
