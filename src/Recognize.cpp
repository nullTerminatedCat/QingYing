#include "Recognize.h"
#include "Ocr.h"
#include "QrDecoder.h"
#include "ResultDialog.h"

#include <QCoreApplication>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QThreadPool>

namespace Recognize {
void ocr(const QImage& image)
{
    auto* dlg = new ResultDialog(QCoreApplication::translate("QingYing", "文字识别"));
    dlg->setTranslateEnabled(true);
    dlg->setStatus(QCoreApplication::translate("QingYing", "正在识别…"));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    QPointer<ResultDialog> guard(dlg);
    // 在全局线程池中识别：退出时 main() 会等待其结束，避免分离线程在 QApplication 析构后回调
    QThreadPool::globalInstance()->start([image, guard] {
        const Ocr::Result r = Ocr::recognize(image);
        QMetaObject::invokeMethod(qApp, [guard, r] {
            if (!guard) return;
            if (!r.ok) {
                guard->setStatus(r.error, true);
                return;
            }
            guard->setText(r.text);
            guard->setStatus(r.text.isEmpty() ? QCoreApplication::translate("QingYing", "未识别到文字（语言：%1）").arg(r.language)
                                              : QCoreApplication::translate("QingYing", "识别语言：%1 · 共 %2 行，可直接编辑后复制")
                                                    .arg(r.language).arg(r.text.count(QLatin1Char('\n')) + 1));
        }, Qt::QueuedConnection);
    });
}
}

namespace Recognize {
void qr(const QImage& image)
{
    auto* dlg = new ResultDialog(QCoreApplication::translate("QingYing", "二维码 / 条码识别"));
    const QList<QrDecoder::Code> codes = QrDecoder::decode(image);
    if (codes.isEmpty()) {
        dlg->setStatus(QCoreApplication::translate("QingYing", "未识别到二维码或条码。请尽量只框选码的区域，并保证清晰。"), true);
    } else {
        QStringList texts;
        for (const auto& c : codes) texts << c.text;
        dlg->setText(texts.join(QStringLiteral("\n\n")));
        QStringList fmts;
        for (const auto& c : codes) fmts << c.format;
        dlg->setStatus(QCoreApplication::translate("QingYing", "识别到 %1 个：%2").arg(codes.size()).arg(fmts.join(QStringLiteral("、"))));
        const QUrl url(codes.first().text.trimmed(), QUrl::StrictMode);
        if (codes.size() == 1 && url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
            dlg->setUrl(url.toString());
    }
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
}
