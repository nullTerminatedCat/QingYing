#include "TranslateText.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace TranslateText {
QUrl chatCompletionsUrl(const QString& baseUrl)
{
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) base.chop(1);
    if (base.isEmpty()) return {};
    if (!base.endsWith(QLatin1String("/chat/completions"), Qt::CaseInsensitive)) base += QStringLiteral("/chat/completions");
    const QUrl url(base, QUrl::StrictMode);
    if (!url.isValid() || url.host().isEmpty()) return {};
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("https") && scheme != QLatin1String("http")) return {};
    return url;
}

QString systemPrompt(bool toChinese)
{
    return QStringLiteral(
               "You are a precise translator. Translate the user's text into %1. "
               "Preserve line breaks, punctuation, numbers, identifiers, and code. "
               "If the text is already in the target language, return it unchanged. "
               "Output only the translation, with no quotes or commentary.")
        .arg(toChinese ? QStringLiteral("Simplified Chinese") : QStringLiteral("English"));
}

QStringList chunkForModel(const QString& text, int limit)
{
    limit = std::max(200, limit);
    QStringList chunks;
    QString cur;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString piece = lines[i] + (i + 1 < lines.size() ? QStringLiteral("\n") : QString());
        if (cur.size() + piece.size() > limit && !cur.isEmpty()) {
            chunks << cur;
            cur.clear();
        }
        if (piece.size() > limit) {
            if (!cur.isEmpty()) {
                chunks << cur;
                cur.clear();
            }
            for (int p = 0; p < piece.size(); p += limit) chunks << piece.mid(p, limit);
        } else {
            cur += piece;
        }
    }
    if (!cur.isEmpty()) chunks << cur;
    return chunks;
}

QString normalizeModelText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("```")) && trimmed.endsWith(QLatin1String("```")) && trimmed.size() > 6) {
        int nl = trimmed.indexOf(QLatin1Char('\n'));
        QString body = nl >= 0 ? trimmed.mid(nl + 1) : trimmed.mid(3);
        if (body.endsWith(QLatin1String("```"))) body.chop(3);
        return body.trimmed();
    }
    return trimmed;
}

QString contentToString(const QJsonValue& content)
{
    if (content.isString()) return content.toString();
    if (content.isArray()) {
        QString out;
        for (const QJsonValue& part : content.toArray()) {
            if (part.isString()) out += part.toString();
            else if (part.isObject()) out += part.toObject().value(QStringLiteral("text")).toString();
        }
        return out;
    }
    return {};
}

QString parseChatCompletion(const QByteArray& body, QString* error)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (error) *error = QStringLiteral("invalid json");
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (!choices.isEmpty()) {
        const QJsonObject message = choices.at(0).toObject().value(QStringLiteral("message")).toObject();
        const QString text = normalizeModelText(contentToString(message.value(QStringLiteral("content"))));
        if (!text.isEmpty()) return text;
    }
    const QJsonObject err = root.value(QStringLiteral("error")).toObject();
    QString message = err.value(QStringLiteral("message")).toString();
    if (message.isEmpty()) message = root.value(QStringLiteral("message")).toString();
    if (message.size() > 240) message = message.left(240);
    if (error) *error = message.isEmpty() ? QStringLiteral("empty completion") : message;
    return {};
}

QString redactSecret(QString text, const QString& secret)
{
    if (!secret.isEmpty() && secret.size() >= 4) text.replace(secret, QStringLiteral("***"));
    return text;
}
}
