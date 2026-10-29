#include "Ocr.h"
#include "OcrModels.h"
#include "OcrWin.h"
#include "RapidOcr.h"
#include "Settings.h"

#include <QCoreApplication>

namespace {
QString clip(QString text)
{
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    if (text.size() > 160) text = text.left(160);
    return text;
}
}

namespace Ocr {
Result recognize(const QImage& image, const std::function<void(const QString&)>& progress)
{
    auto note = [&](const QString& text) {
        if (progress) progress(text);
    };
    if (image.isNull() || image.width() < 2 || image.height() < 2) {
        Result r;
        r.error = QCoreApplication::translate("QingYing", "没有可识别的图像");
        return r;
    }
    if (Settings::ocrEngine() == QLatin1String("system")) return OcrWin::recognize(image);

    auto fallback = [&](const QString& why) {
        Result sys = OcrWin::recognize(image);
        if (sys.ok) {
            sys.fellBack = true;
            sys.note = why;
            return sys;
        }
        Result r;
        r.error = why.isEmpty() ? sys.error : QCoreApplication::translate("QingYing", "%1（系统 OCR：%2）").arg(why, sys.error);
        return r;
    };

    if (!RapidOcr::runtimeAvailable())
        return fallback(QCoreApplication::translate("QingYing", "当前构建未包含 ONNX Runtime，已改用系统 OCR。"));

    if (!OcrModels::locate().ready) {
        QString err;
        if (!OcrModels::ensure(&err, progress)) {
            const QString why = QCoreApplication::translate("QingYing", "RapidOCR 模型不可用（%1），已改用系统 OCR。").arg(clip(err));
            return fallback(why);
        }
    }
    note(QCoreApplication::translate("QingYing", "正在识别…"));
    Result rapid = RapidOcr::recognize(image);
    if (!rapid.ok) {
        const QString why = QCoreApplication::translate("QingYing", "RapidOCR 失败（%1），已改用系统 OCR。").arg(clip(rapid.error));
        return fallback(why);
    }
    return rapid;
}
}
