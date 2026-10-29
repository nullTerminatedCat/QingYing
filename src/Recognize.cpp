#include "Recognize.h"
#include "Ocr.h"
#include "QrDecoder.h"
#include "ResultDialog.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QPointer>
#include <QScreen>
#include <QStringList>
#include <QUrl>
#include <QThreadPool>

namespace {
void placeBeside(QWidget* w, const QRect& anchor)
{
    if (!w || anchor.isNull() || !anchor.isValid()) return;
    QScreen* screen = QGuiApplication::screenAt(anchor.center());
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    const QSize sz = w->size().expandedTo(QSize(560, 460));
    int x = anchor.right() + 16;
    int y = anchor.top();
    if (x + sz.width() > avail.right() - 8) x = anchor.left() - 16 - sz.width();
    if (x < avail.left() + 8) x = avail.left() + 8;
    if (y + sz.height() > avail.bottom() - 8) y = avail.bottom() - sz.height() - 8;
    if (y < avail.top() + 8) y = avail.top() + 8;
    w->move(x, y);
}
}

namespace Recognize {
void showOcr(const QImage& image, bool autoTranslate, bool focusAsk, bool rewrite, AskText::Mode mode, const QRect& anchor)
{
    const QString title = focusAsk ? QCoreApplication::translate("QingYing", "截图秒答")
                         : rewrite  ? QCoreApplication::translate("QingYing", "改文案")
                         : autoTranslate ? QCoreApplication::translate("QingYing", "识别并翻译")
                                         : QCoreApplication::translate("QingYing", "文字识别");
    auto* dlg = new ResultDialog(title);
    dlg->setTranslateEnabled(true);
    dlg->setAssistEnabled(true);
    dlg->setWorking(true);
    dlg->setStatus(QCoreApplication::translate("QingYing", "正在识别，请稍候…"));
    dlg->resize(560, 460);
    placeBeside(dlg, anchor);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
    QPointer<ResultDialog> guard(dlg);
    // 在全局线程池中识别：退出时 main() 会等待其结束，避免分离线程在 QApplication 析构后回调
    QThreadPool::globalInstance()->start([image, guard, autoTranslate, focusAsk, rewrite, mode] {
        const Ocr::Result r = Ocr::recognize(image, [guard](const QString& status) {
            QMetaObject::invokeMethod(qApp, [guard, status] {
                if (guard) guard->setStatus(status);
            }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(qApp, [guard, r, autoTranslate, focusAsk, rewrite, mode] {
            if (!guard) return;
            guard->setWorking(false);
            if (!r.ok) {
                guard->setStatus(r.error, true);
                return;
            }
            guard->setText(r.text);
            const bool empty = r.text.trimmed().isEmpty();
            QString base = empty
                               ? QCoreApplication::translate("QingYing", "未识别到文字（%1）。可换更清晰的选区再试。").arg(r.language)
                               : QCoreApplication::translate("QingYing", "识别语言：%1 · 共 %2 行，可直接编辑后复制")
                                     .arg(r.language)
                                     .arg(r.text.count(QLatin1Char('\n')) + 1);
            if (r.fellBack && !r.note.isEmpty()) base = r.note + QLatin1Char(' ') + base;
            if (empty && (focusAsk || rewrite))
                base += QCoreApplication::translate("QingYing", " 未请求 AI。");
            guard->setStatus(base);
            // 空结果不翻译、不提问、不改写。自动翻译走 ResultDialog::translateSuggested，跳过与缓存同样生效。
            if (empty) return;
            if (rewrite) guard->startAssist(mode);
            else if (focusAsk) {
                guard->setStatus(base + QCoreApplication::translate("QingYing", " 在下方输入问题后点「提问」。"));
                guard->focusQuestion();
            } else if (autoTranslate)
                guard->translateSuggested();
        }, Qt::QueuedConnection);
    });
}

void ocr(const QImage& image, const QRect& anchor) { showOcr(image, false, false, false, AskText::Mode::Polish, anchor); }
void ocrAndTranslate(const QImage& image, const QRect& anchor) { showOcr(image, true, false, false, AskText::Mode::Polish, anchor); }
void ocrAndAsk(const QImage& image, const QRect& anchor) { showOcr(image, false, true, false, AskText::Mode::Ask, anchor); }
void ocrAndRewrite(const QImage& image, AskText::Mode mode, const QRect& anchor)
{
    showOcr(image, false, false, true, mode, anchor);
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