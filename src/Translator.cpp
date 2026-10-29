#include <QCoreApplication>
#include "Translator.h"
#include "Settings.h"
#include "Logic.h"

#include "TranslateText.h"

#include <QList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>

namespace {
TranslateText::TranslationMemory g_recall;
unsigned g_recallEpoch = 0;

QNetworkAccessManager* net()
{
    static auto* n = new QNetworkAccessManager(qApp);
    return n;
}

QList<QPointer<QNetworkReply>> g_inflight;

void trackReply(QNetworkReply* reply)
{
    QList<QPointer<QNetworkReply>> live;
    for (const QPointer<QNetworkReply>& r : g_inflight)
        if (r) live.append(r);
    if (reply) live.append(reply);
    g_inflight = live;
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
    trackReply(reply);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, pair, done] {
        reply->deleteLater();
        Translator::Result r;
        r.pair = pair;
        if (reply->error() != QNetworkReply::NoError) {
            switch (reply->error()) {
            case QNetworkReply::OperationCanceledError:
                r.error = QCoreApplication::translate("QingYing", "已取消");
                break;
            case QNetworkReply::TimeoutError:
                r.error = QCoreApplication::translate("QingYing", "连接超时，请检查网络后重试");
                break;
            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::NetworkSessionFailedError:
                r.error = QCoreApplication::translate("QingYing", "无法连接翻译服务，请检查网络");
                break;
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::TemporaryNetworkFailureError:
                r.error = QCoreApplication::translate("QingYing", "网络暂时不可用，请稍后重试");
                break;
            case QNetworkReply::SslHandshakeFailedError:
                r.error = QCoreApplication::translate("QingYing", "安全连接失败，请检查系统时间或代理设置");
                break;
            case QNetworkReply::ContentAccessDenied:
            case QNetworkReply::AuthenticationRequiredError:
                r.error = QCoreApplication::translate("QingYing", "翻译服务拒绝访问，可能已达免费额度，请稍后再试");
                break;
            default:
                r.error = reply->errorString();
                break;
            }
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
            r.error = QCoreApplication::translate("QingYing", "翻译服务繁忙或已达免费额度，请稍后再试");
            done(r);
            return;
        }
        r.ok = true;
        r.text = decodeEntities(translated);
        done(r);
    });
}

QString aiFailure(QNetworkReply* reply, const QString& key)
{
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    QString parsed;
    TranslateText::parseChatCompletion(body, &parsed);
    parsed = TranslateText::redactSecret(parsed, key);
    if (reply->error() == QNetworkReply::TimeoutError)
        return QCoreApplication::translate("QingYing", "连接超时，请检查网络或接口地址");
    if (reply->error() == QNetworkReply::HostNotFoundError || reply->error() == QNetworkReply::NetworkSessionFailedError)
        return QCoreApplication::translate("QingYing", "无法连接 AI 接口，请检查地址和网络");
    if (http == 401 || http == 403)
        return QCoreApplication::translate("QingYing", "API 密钥被拒绝（HTTP %1）").arg(http);
    if (!parsed.isEmpty() && parsed != QLatin1String("invalid json") && parsed != QLatin1String("empty completion"))
        return QCoreApplication::translate("QingYing", "AI 接口返回错误（HTTP %1）：%2").arg(http).arg(parsed);
    if (reply->error() != QNetworkReply::NoError)
        return TranslateText::redactSecret(reply->errorString(), key);
    return QCoreApplication::translate("QingYing", "AI 没有返回文字");
}

void translateAiChunk(const QString& chunk, bool toChinese, std::function<void(Translator::Result)> done)
{
    const QUrl url = TranslateText::chatCompletionsUrl(Settings::aiBaseUrl());
    const QString key = Settings::aiApiKey();
    const QString model = Settings::aiModel();
    if (!url.isValid() || key.isEmpty() || model.isEmpty()) {
        Translator::Result r;
        r.error = QCoreApplication::translate("QingYing", "请在设置中填写 AI 翻译的接口地址、密钥和模型");
        done(r);
        return;
    }
    QJsonObject system;
    system.insert(QStringLiteral("role"), QStringLiteral("system"));
    system.insert(QStringLiteral("content"), TranslateText::systemPrompt(toChinese));
    QJsonObject user;
    user.insert(QStringLiteral("role"), QStringLiteral("user"));
    user.insert(QStringLiteral("content"), chunk);
    QJsonArray messages;
    messages.append(system);
    messages.append(user);
    QJsonObject body;
    body.insert(QStringLiteral("model"), model);
    body.insert(QStringLiteral("temperature"), 0.2);
    body.insert(QStringLiteral("stream"), false);
    body.insert(QStringLiteral("messages"), messages);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QByteArray("Bearer ") + key.toUtf8());
    req.setTransferTimeout(45000);
    QNetworkReply* reply = net()->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    trackReply(reply);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, key, done] {
        reply->deleteLater();
        Translator::Result r;
        r.engine = QStringLiteral("ai");
        if (reply->error() != QNetworkReply::NoError) {
            r.error = aiFailure(reply, key);
            done(r);
            return;
        }
        QString err;
        const QString text = TranslateText::parseChatCompletion(reply->readAll(), &err);
        if (text.isEmpty()) {
            err = TranslateText::redactSecret(err, key);
            r.error = (err.isEmpty() || err == QLatin1String("empty completion"))
                          ? QCoreApplication::translate("QingYing", "AI 没有返回文字")
                          : QCoreApplication::translate("QingYing", "AI 返回无法使用：%1").arg(err);
            done(r);
            return;
        }
        r.ok = true;
        r.text = text;
        done(r);
    });
}

void translateAiChunks(QStringList chunks, bool toChinese, QStringList doneParts, std::function<void(Translator::Result)> done)
{
    if (chunks.isEmpty()) {
        Translator::Result r;
        r.ok = true;
        r.engine = QStringLiteral("ai");
        r.text = doneParts.join(QString());
        done(r);
        return;
    }
    const QString next = chunks.takeFirst();
    translateAiChunk(next, toChinese, [chunks, toChinese, doneParts, done](Translator::Result part) mutable {
        if (!part.ok) {
            done(part);
            return;
        }
        doneParts << part.text;
        translateAiChunks(std::move(chunks), toChinese, std::move(doneParts), std::move(done));
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

void runMemory(const QString& text, Target target, bool fellBack, std::function<void(Result)> done)
{
    const QString pair = QStringLiteral("autodetect|") + langCode(target);
    translateChunks(chunkText(text), pair, {}, [fellBack, done = std::move(done)](Result r) {
        r.engine = QStringLiteral("mymemory");
        r.fellBack = fellBack;
        done(r);
    });
}

void forgetRecall()
{
    g_recall = {};
    ++g_recallEpoch;
}

bool textIsCachedTranslation(const QString& text)
{
    return TranslateText::isCachedOutput(text, g_recall);
}

void cancelInFlight()
{
    // 先断开再 abort：finished 是同步的，析构或换文本时不能再回调进界面。
    // 分块翻译停在当前块，不会再发下一块。已经送到服务端的这一次仍可能计费。
    const QList<QPointer<QNetworkReply>> copy = g_inflight;
    g_inflight.clear();
    for (const QPointer<QNetworkReply>& reply : copy) {
        if (!reply || reply->isFinished()) continue;
        reply->disconnect();
        reply->abort();
        reply->deleteLater();
    }
}

void translate(const QString& text, Target target, std::function<void(Result)> done)
{
    if (!Settings::translateEnabled()) {
        Result r;
        r.error = QCoreApplication::translate("QingYing", "已在设置中关闭联网翻译");
        done(r);
        return;
    }
    const bool toChinese = target == Target::Chinese;
    const TranslateText::Reuse saved = TranslateText::reuseTranslation(text, toChinese, g_recall);
    if (saved.hit) {
        Result r;
        r.ok = true;
        r.text = saved.text;
        r.engine = QStringLiteral("local");
        r.local = saved.kind;
        done(r);
        return;
    }
    const unsigned epoch = g_recallEpoch;
    auto finish = [text, toChinese, epoch, done = std::move(done)](Result r) {
        // 识别面板已换了新文本时，不要把这次结果记成下一次的往返缓存
        if (epoch == g_recallEpoch && r.ok && r.local == TranslateText::ReuseKind::None)
            TranslateText::rememberTranslation(g_recall, text, toChinese, r.text);
        done(r);
    };
    const bool wantAi = Settings::aiTranslateEnabled();
    if (wantAi && Settings::aiTranslateReady()) {
        // 默认约 3500 字一块。不要改成 MyMemory 的 450，否则长文会拆成许多次请求。
        translateAiChunks(TranslateText::chunkForModel(text), toChinese, {}, [text, target, finish](Result r) {
            if (r.ok || !Settings::memoryFallback()) {
                finish(r);
                return;
            }
            runMemory(text, target, true, finish);
        });
        return;
    }
    if (wantAi && !Settings::memoryFallback()) {
        Result r;
        r.error = QCoreApplication::translate("QingYing", "请在设置中填写 AI 翻译的接口地址、密钥和模型，或打开 MyMemory 兜底");
        finish(r);
        return;
    }
    runMemory(text, target, false, std::move(finish));
}
}