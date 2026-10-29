#include "RapidOcr.h"
#include "OcrModels.h"
#include "OcrPost.h"

#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

#ifndef QINGYING_HAS_ONNXRUNTIME

namespace RapidOcr {
bool runtimeAvailable() { return false; }
Ocr::Result recognize(const QImage&)
{
    Ocr::Result r;
    r.error = QCoreApplication::translate("QingYing", "当前构建未链接 ONNX Runtime");
    return r;
}
}

#else

#include <onnxruntime_cxx_api.h>

namespace {
struct Session {
    std::unique_ptr<Ort::Session> session;
    std::string input;
    std::string output;
    std::vector<int64_t> inShape;
};

Ort::Env& env()
{
    static Ort::Env e(ORT_LOGGING_LEVEL_ERROR, "qingying");
    return e;
}

std::mutex& gate()
{
    static std::mutex m;
    return m;
}

Session openSession(const QString& path)
{
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(4);
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    Session s;
#ifdef _WIN32
    s.session = std::make_unique<Ort::Session>(env(), path.toStdWString().c_str(), opts);
#else
    s.session = std::make_unique<Ort::Session>(env(), path.toStdString().c_str(), opts);
#endif
    Ort::AllocatorWithDefaultOptions alloc;
    s.input = s.session->GetInputNameAllocated(0, alloc).get();
    s.output = s.session->GetOutputNameAllocated(0, alloc).get();
    s.inShape = s.session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    return s;
}

std::vector<int64_t> run(Session& model, const std::vector<float>& input, const std::vector<int64_t>& shape, std::vector<float>& output)
{
    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, const_cast<float*>(input.data()), input.size(), shape.data(), shape.size());
    const char* inNames[] = {model.input.c_str()};
    const char* outNames[] = {model.output.c_str()};
    auto outs = model.session->Run(Ort::RunOptions{nullptr}, inNames, &tensor, 1, outNames, 1);
    auto info = outs[0].GetTensorTypeAndShapeInfo();
    const size_t count = info.GetElementCount();
    float* data = outs[0].GetTensorMutableData<float>();
    output.assign(data, data + count);
    return info.GetShape();
}

void fillDet(const QImage& rgb, std::vector<float>& dst)
{
    const int w = rgb.width(), h = rgb.height();
    dst.assign(size_t(3 * h * w), 0.f);
    const float mean[3] = {0.485f, 0.456f, 0.406f};
    const float stdv[3] = {0.229f, 0.224f, 0.225f};
    for (int y = 0; y < h; ++y) {
        const uchar* row = rgb.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < 3; ++c) {
                const float v = row[x * 3 + c] / 255.f;
                dst[size_t((c * h + y) * w + x)] = (v - mean[c]) / stdv[c];
            }
        }
    }
}

void fillNorm(const QImage& rgb, int targetW, std::vector<float>& dst)
{
    const int h = rgb.height(), w = rgb.width();
    dst.assign(size_t(3 * h * targetW), -1.f);
    for (int y = 0; y < h; ++y) {
        const uchar* row = rgb.constScanLine(y);
        for (int x = 0; x < w && x < targetW; ++x) {
            for (int c = 0; c < 3; ++c) {
                const float v = row[x * 3 + c] / 255.f;
                dst[size_t((c * h + y) * targetW + x)] = (v - 0.5f) / 0.5f;
            }
        }
    }
}

QImage fitHeight(const QImage& src, int height, int maxWidth)
{
    if (src.isNull() || src.width() < 1 || src.height() < 1) return {};
    int width = int(std::ceil(height * (double(src.width()) / src.height())));
    width = std::clamp(width, 8, std::max(8, maxWidth));
    return src.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
}

void maybeSigmoid(std::vector<float>& map)
{
    for (float v : map) {
        if (v < -0.01f || v > 1.01f) {
            for (float& x : map) x = 1.f / (1.f + std::exp(-x));
            return;
        }
    }
}

bool rotate180(const float* out, size_t n)
{
    if (n < 2) return false;
    float a = out[0], b = out[1];
    if (a < 0.f || b < 0.f || std::abs(a + b - 1.f) > 0.05f) {
        const float m = std::max(a, b);
        a = std::exp(a - m);
        b = std::exp(b - m);
        const float s = a + b;
        a /= s;
        b /= s;
    }
    return b > a && b >= 0.9f;
}

QRectF boundsOf(const OcrPost::Quad& q)
{
    float minx = q.p[0].x, maxx = minx, miny = q.p[0].y, maxy = miny;
    for (const auto& p : q.p) {
        minx = std::min(minx, p.x);
        maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y);
        maxy = std::max(maxy, p.y);
    }
    return QRectF(minx, miny, std::max(1.f, maxx - minx), std::max(1.f, maxy - miny));
}

struct Engine {
    Session det, rec, cls;
    bool hasCls = false;
    QString loadedKey;
    QStringList dict;
};

Engine& engine()
{
    static Engine e;
    return e;
}

void load(const OcrModels::Paths& paths)
{
    const QString key = paths.det + QLatin1Char('|') + paths.rec + QLatin1Char('|') + paths.cls + QLatin1Char('|') + paths.dict;
    Engine& e = engine();
    if (e.loadedKey == key && e.det.session && e.rec.session) return;
    e = Engine();
    e.det = openSession(paths.det);
    e.rec = openSession(paths.rec);
    if (!paths.cls.isEmpty()) {
        e.cls = openSession(paths.cls);
        e.hasCls = true;
    }
    QFile dictFile(paths.dict);
    if (!dictFile.open(QIODevice::ReadOnly)) throw std::runtime_error("无法读取识别字典");
    e.dict = OcrPost::dictFromUtf8(dictFile.readAll());
    if (e.dict.size() < 100) throw std::runtime_error("识别字典无效");
    e.loadedKey = key;
}
} // namespace

namespace RapidOcr {
bool runtimeAvailable() { return true; }

Ocr::Result recognize(const QImage& input)
{
    Ocr::Result r;
    const OcrModels::Paths paths = OcrModels::locate();
    if (!paths.ready) {
        r.error = QCoreApplication::translate("QingYing", "未找到 RapidOCR 模型");
        return r;
    }
    try {
        std::lock_guard<std::mutex> lock(gate());
        load(paths);
        QImage img = input.convertToFormat(QImage::Format_RGB888);
        img.setDevicePixelRatio(1);
        if (std::max(img.width(), img.height()) > 4096)
            img = img.scaled(4096, 4096, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);

        OcrPost::DetSize size = OcrPost::detInputSize(img.width(), img.height(), 960);
        const auto& detShape = engine().det.inShape;
        if (detShape.size() >= 4 && detShape[2] > 0 && detShape[3] > 0) {
            size.height = int(detShape[2]);
            size.width = int(detShape[3]);
        }
        const QImage detImg = img.scaled(size.width, size.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
        std::vector<float> detIn, detOut;
        fillDet(detImg, detIn);
        const std::vector<int64_t> detDims = run(engine().det, detIn, {1, 3, detImg.height(), detImg.width()}, detOut);
        int mapH = detImg.height(), mapW = detImg.width();
        if (detDims.size() == 4) {
            mapH = int(detDims[2]);
            mapW = int(detDims[3]);
        } else if (detDims.size() == 3) {
            mapH = int(detDims[1]);
            mapW = int(detDims[2]);
        }
        if (mapH < 1 || mapW < 1 || int64_t(mapH) * mapW > int64_t(detOut.size())) {
            r.error = QCoreApplication::translate("QingYing", "检测模型输出异常");
            return r;
        }
        maybeSigmoid(detOut);
        const auto boxes = OcrPost::detectBoxes(detOut.data(), mapW, mapH, img.width(), img.height());

        const auto& recShape = engine().rec.inShape;
        const int recH = (recShape.size() >= 3 && recShape[2] > 0) ? int(recShape[2]) : 48;
        const int fixedW = (recShape.size() >= 4 && recShape[3] > 0) ? int(recShape[3]) : 0;
        const int maxW = fixedW > 0 ? fixedW : 960;
        bool dictChecked = false;
        QList<OcrPost::LinePiece> lines;
        for (const auto& box : boxes) {
            QImage crop = OcrPost::warpQuad(img, box.quad);
            if (crop.isNull() || crop.width() < 2 || crop.height() < 2) continue;
            if (engine().hasCls) {
                const QImage clsImg = fitHeight(crop, 48, 192);
                std::vector<float> clsIn, clsOut;
                fillNorm(clsImg, 192, clsIn);
                run(engine().cls, clsIn, {1, 3, clsImg.height(), 192}, clsOut);
                if (rotate180(clsOut.data(), clsOut.size())) crop = crop.transformed(QTransform().rotate(180)).convertToFormat(QImage::Format_RGB888);
            }
            const QImage recImg = fitHeight(crop, recH, maxW);
            if (recImg.isNull()) continue;
            const int tensorW = fixedW > 0 ? fixedW : recImg.width();
            std::vector<float> recIn, recOut;
            fillNorm(recImg, tensorW, recIn);
            const std::vector<int64_t> recDims = run(engine().rec, recIn, {1, 3, recImg.height(), tensorW}, recOut);
            int time = 0, classes = 0;
            if (recDims.size() == 3) {
                time = int(recDims[1]);
                classes = int(recDims[2]);
            } else if (recDims.size() == 2) {
                time = int(recDims[0]);
                classes = int(recDims[1]);
            }
            if (time < 1 || classes < 2 || int64_t(time) * classes > int64_t(recOut.size())) continue;
            if (!dictChecked) {
                if (classes == engine().dict.size() + 2) engine().dict << QStringLiteral(" ");
                else if (classes != engine().dict.size() + 1) {
                    r.error = QCoreApplication::translate("QingYing", "识别字典与模型不匹配（%1 / %2）").arg(classes).arg(engine().dict.size());
                    return r;
                }
                dictChecked = true;
            }
            const QString text = OcrPost::ctcGreedy(recOut.data(), time, classes, engine().dict);
            if (!text.trimmed().isEmpty()) lines.push_back({boundsOf(box.quad), text});
        }
        r.text = OcrPost::joinLines(std::move(lines));
        r.language = QCoreApplication::translate("QingYing", "RapidOCR · 简体中文");
        r.ok = true;
    } catch (const Ort::Exception& e) {
        r.error = QCoreApplication::translate("QingYing", "RapidOCR 失败：%1").arg(QString::fromUtf8(e.what()));
    } catch (const std::exception& e) {
        r.error = QCoreApplication::translate("QingYing", "RapidOCR 失败：%1").arg(QString::fromUtf8(e.what()));
    }
    return r;
}
}

#endif
