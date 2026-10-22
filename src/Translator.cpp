#include <QCoreApplication>
#include "Translator.h"
#include "Settings.h"
#include "Logic.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace {
QNetworkAccessManager* net()
{
    static auto* n = new QNetworkAccessManager(qApp);
    return n;
}

QString langCode(Translator::Target t)
{
    return t == Translator::Target::Chinese ? QStringLiteral("zh-CN") : QStringLiteral("en");
}

// MyMemory 单次建议不超过约 500 字符；按行打包，尽量保留换行
QStringList chunkText(const QString& text)
{
    QStringList chunks;
    QString cur;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString piece = lines[i] + (i + 1 < lines.size() ? QStringLiteral("\n") : QString());
        if (cur.size() + piece.size() > 450 && !cur.isEmpty()) {
            chunks << cur;
            cur.clear();
        }
        // 单行超长则硬切
        if (piece.size() > 450) {
            if (!cur.isEmpty()) {
                chunks << cur;
                cur.clear();
            }
            for (int p = 0; p < piece.size(); p += 450) chunks << piece.mid(p, 450);
        } else {
            cur += piece;
        }
    }
    if (!cur.isEmpty()) chunks << cur;
    return chunks;
}

QString decodeEntities(QString s)
{
    s.replace(QLatin1String("&quot;"), QLatin1String("\""));
    s.replace(QLatin1String("&#39;"), QLatin1String("'"));
    s.replace(QLatin1String("&amp;"), QLatin1String("&"));
    s.replace(QLatin1String("&lt;"), QLatin1String("<"));
    s.replace(QLatin1String("&gt;"), QLatin1String(">"));
    return s;
}

void translateChunk(const QString& chunk, const QString& pair, std::function<void(Translator::Result)> done)
{
    QUrl url(QStringLiteral("https://api.mymemory.translated.net/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), chunk);
    q.addQueryItem(QStringLiteral("langpair"), pair);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("QingYing/%1").arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(20000);
    QNetworkReply* reply = net()->get(req);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, pair, done] {
        reply->deleteLater();
        Translator::Result r;
        r.pair = pair;
        if (reply->error() != QNetworkReply::NoError) {
            r.error = reply->errorString();
            done(r);
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const int status = root.value(QStringLiteral("responseStatus")).toInt();
        const QString translated =
            root.value(QStringLiteral("responseData")).toObject().value(QStringLiteral("translatedText")).toString();
        if (status != 200 || translated.isEmpty()) {
            const QString detail = root.value(QStringLiteral("responseDetails")).toString();
            r.error = detail.isEmpty() ? QCoreApplication::translate("QingYing", "翻译服务未返回有效结果") : detail;
            done(r);
            return;
        }
        // 配额用尽时接口常把说明写进 translatedText
        if (translated.contains(QLatin1String("MYMEMORY WARNING"), Qt::CaseInsensitive)
            || translated.contains(QLatin1String("QUERY LENGTH LIMIT"), Qt::CaseInsensitive)) {
            r.error = translated;
            done(r);
            return;
        }
        r.ok = true;
        r.text = decodeEntities(translated);
        done(r);
    });
}

void translateChunks(QStringList chunks, const QString& pair, QStringList doneParts,
                     std::function<void(Translator::Result)> done)
{
    if (chunks.isEmpty()) {
        Translator::Result r;
        r.ok = true;
        r.pair = pair;
        r.text = doneParts.join(QString());
        done(r);
        return;
    }
    const QString next = chunks.takeFirst();
    translateChunk(next, pair, [chunks, pair, doneParts, done](Translator::Result part) mutable {
        if (!part.ok) {
            done(part);
            return;
        }
        doneParts << part.text;
        translateChunks(chunks, pair, doneParts, done);
    });
}
} // namespace

namespace Translator {
Target suggestTarget(const QString& text)
{
    return Logic::mostlyCjk(text) ? Target::English : Target::Chinese;
}

void translate(const QString& text, Target target, std::function<void(Result)> done)
{
    if (!Settings::translateEnabled()) {
        Result r;
        r.error = QCoreApplication::translate("QingYing", "已在设置中关闭联网翻译");
        done(r);
        return;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        Result r;
        r.error = QCoreApplication::translate("QingYing", "没有可翻译的文字");
        done(r);
        return;
    }
    // autodetect 由 MyMemory 判断源语言；显式目标为 zh-CN 或 en
    const QString pair = QStringLiteral("autodetect|") + langCode(target);
    translateChunks(chunkText(text), pair, {}, std::move(done));
}
}