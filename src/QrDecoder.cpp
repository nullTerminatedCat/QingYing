#include "QrDecoder.h"

#include "ReadBarcode.h"

namespace QrDecoder {
QList<Code> decode(const QImage& input)
{
    QList<Code> out;
    if (input.isNull()) return out;
    const QImage img = input.convertToFormat(QImage::Format_RGB32);
    const ZXing::ImageView view(img.constBits(), img.width(), img.height(), ZXing::ImageFormat::BGRX, int(img.bytesPerLine()));
    ZXing::ReaderOptions opts;
    opts.setTryHarder(true);
    opts.setTryRotate(true);
    opts.setTryInvert(true);
    for (const auto& r : ZXing::ReadBarcodes(view, opts)) {
        if (!r.isValid()) continue;
        out.append({QString::fromStdString(ZXing::ToString(r.format())), QString::fromStdString(r.text())});
    }
    return out;
}
}
