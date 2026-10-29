#include "OcrWin.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <QRegularExpression>
#include <QStringList>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

using namespace winrt;
using namespace winrt::Windows::Globalization;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Storage::Streams;

namespace {
QString fromH(const hstring& h) { return QString::fromWCharArray(h.c_str(), int(h.size())); }

// Windows OCR 会在中日韩字符之间插入空格，去掉它们
QString tidy(QString line)
{
    static const QRegularExpression cjkSpace(
        QStringLiteral("(?<=[\\x{2E80}-\\x{9FFF}\\x{F900}-\\x{FAFF}\\x{FF00}-\\x{FFEF}\\x{3000}-\\x{303F}]) +(?=[\\x{2E80}-\\x{9FFF}\\x{F900}-\\x{FAFF}\\x{FF00}-\\x{FFEF}\\x{3000}-\\x{303F}])"));
    return line.replace(cjkSpace, QString());
}
}
#endif

namespace OcrWin {
Ocr::Result recognize(const QImage& input)
{
    Ocr::Result r;
#ifdef Q_OS_WIN
    try {
        try {
            init_apartment(apartment_type::multi_threaded);
        } catch (const hresult_error&) {
        }
        OcrEngine engine{nullptr};
        const Language zh(L"zh-Hans");
        if (OcrEngine::IsLanguageSupported(zh)) engine = OcrEngine::TryCreateFromLanguage(zh);
        if (!engine) engine = OcrEngine::TryCreateFromUserProfileLanguages();
        if (!engine) {
            r.error = QCoreApplication::translate("QingYing", "系统没有可用的 OCR 语言包。请在“设置 → 时间和语言 → 语言和区域”中添加“中文(简体)”，"
                                                              "并在语言选项里安装“光学字符识别”功能后重试。");
            return r;
        }
        QImage img = input.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        img.setDevicePixelRatio(1);
        const int maxDim = int(OcrEngine::MaxImageDimension());
        if (qMax(img.width(), img.height()) > maxDim)
            img = img.scaled(maxDim, maxDim, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        else if (qMin(img.width(), img.height()) < 120 && qMax(img.width(), img.height()) * 2 <= maxDim)
            img = img.scaled(img.size() * 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        DataWriter writer;
        const auto* bits = img.constBits();
        writer.WriteBytes(array_view<const uint8_t>(bits, bits + img.sizeInBytes()));
        const IBuffer buffer = writer.DetachBuffer();
        const SoftwareBitmap bmp = SoftwareBitmap::CreateCopyFromBuffer(buffer, BitmapPixelFormat::Bgra8, img.width(), img.height(),
                                                                        BitmapAlphaMode::Premultiplied);
        const OcrResult res = engine.RecognizeAsync(bmp).get();
        QStringList lines;
        for (const auto& line : res.Lines()) lines << tidy(fromH(line.Text()));
        r.text = lines.join(QLatin1Char('\n'));
        r.language = fromH(engine.RecognizerLanguage().DisplayName());
        r.ok = true;
    } catch (const hresult_error& e) {
        r.error = QCoreApplication::translate("QingYing", "OCR 失败：%1").arg(fromH(e.message()));
    } catch (...) {
        r.error = QCoreApplication::translate("QingYing", "OCR 失败：未知错误");
    }
#else
    Q_UNUSED(input);
    r.error = QCoreApplication::translate("QingYing", "系统 OCR 仅在 Windows 上可用");
#endif
    return r;
}
}
