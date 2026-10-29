#pragma once
#include <QImage>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <array>

// PP-OCRv4 的纯算法部分（检测后处理、裁切、CTC），不依赖 ONNX Runtime，便于单测。
namespace OcrPost {
struct Vec2 {
    float x = 0;
    float y = 0;
};
// 四点顺序：左上、右上、右下、左下
struct Quad {
    std::array<Vec2, 4> p{};
};

struct TextBox {
    Quad quad;
    float score = 0;
};

struct LinePiece {
    QRectF box;
    QString text;
};

struct DetSize {
    int width = 0;
    int height = 0;
};

// 长边不超过 limit，宽高四舍五入到 32 的倍数（至少 32），与 PaddleOCR DB 预处理一致
DetSize detInputSize(int width, int height, int limit = 960);

// prob 为行优先的检测概率图。返回框已映射回原图像素坐标。
QList<TextBox> detectBoxes(const float* prob, int width, int height, int origWidth, int origHeight, float thresh = 0.3f,
                           float boxThresh = 0.5f, float unclipRatio = 1.6f);

// 把四边形透视成水平文本条；高宽比 ≥ 1.5 时逆时针转 90°（竖排）
QImage warpQuad(const QImage& src, const Quad& quad);

// 贪婪 CTC。logits 行优先 [time, classes]，类别 0 为 blank，1.. 对应字典
QString ctcGreedy(const float* logits, int timeSteps, int classes, const QStringList& dict);

QStringList dictFromUtf8(const QByteArray& utf8);

// 同一行的框用空格隔开（间隙够大时），行与行用换行
QString joinLines(QList<LinePiece> pieces);
}
