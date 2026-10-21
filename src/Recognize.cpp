#include "Recognize.h"
#include "Ocr.h"
#include "ResultDialog.h"

#include <QCoreApplication>
#include <QPointer>
#include <QStringList>
#include <thread>

namespace Recognize {
void ocr(const QImage& image)
{
    auto* dlg = new ResultDialog(QStringLiteral("文字识别"));
    dlg->setStatus(QStringLiteral("正在识别…"));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    QPointer<ResultDialog> guard(dlg);
    std::thread([image, guard] {
        const Ocr::Result r = Ocr::recognize(image);
        QMetaObject::invokeMethod(qApp, [guard, r] {
            if (!guard) return;
            if (!r.ok) {
                guard->setStatus(r.error, true);
                return;
            }
            guard->setText(r.text);
            guard->setStatus(r.text.isEmpty() ? QStringLiteral("未识别到文字（语言：%1）").arg(r.language)
                                              : QStringLiteral("识别语言：%1 · 共 %2 行，可直接编辑后复制")
                                                    .arg(r.language).arg(r.text.count(QLatin1Char('\n')) + 1));
        }, Qt::QueuedConnection);
    }).detach();
}
}

namespace Recognize {
void qr(const QImage&)
{
    auto* dlg = new ResultDialog(QStringLiteral("二维码识别"));
    dlg->setStatus(QStringLiteral("二维码识别即将推出"), true);
    dlg->show();
}
}
