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

bool isFullwidthLatin(uint u)
{
    return (u >= 0xFF21 && u <= 0xFF3A) || (u >= 0xFF41 && u <= 0xFF5A);
}

bool alreadyInTargetLanguage(const QString& text, bool toChinese)
{
    if (text.trimmed().isEmpty()) return false;
    int han = 0;
    int latin = 0;
    int otherLetters = 0;
    for (const QChar ch : text) {
        if (ch.isHighSurrogate() || ch.isLowSurrogate()) continue;
        const bool fullwidthLatin = isFullwidthLatin(ch.unicode());
        if (ch.script() == QChar::Script_Han)
            ++han;
        else if (ch.script() == QChar::Script_Latin || fullwidthLatin)
            ++latin;
        else if (ch.isLetter())
            ++otherLetters;
    }
    // 假名、谚文、西里尔字母等：不是「已经是中文或英文」
    if (otherLetters > 0) return false;
    if (han == 0 && latin == 0) return true;
    if (toChinese) return han > 0 && latin == 0;
    return latin > 0 && han == 0;
}

Reuse reuseTranslation(const QString& text, bool toChinese, const TranslationMemory& memory)
{
    Reuse out;
    if (text.trimmed().isEmpty()) return out;
    if (alreadyInTargetLanguage(text, toChinese)) {
        out.hit = true;
        out.kind = ReuseKind::Already;
        out.text = text;
        return out;
    }
    if (!memory.valid) return out;
    const QString cur = text.trimmed();
    const bool back = !memory.translated.trimmed().isEmpty() && !memory.source.trimmed().isEmpty()
                      && cur == memory.translated.trimmed() && memory.toChinese != toChinese;
    if (back) {
        out.hit = true;
        out.kind = ReuseKind::RoundTrip;
        out.text = memory.source;
        return out;
    }
    if (!memory.translated.trimmed().isEmpty() && !memory.source.trimmed().isEmpty() && cur == memory.source.trimmed()
        && memory.toChinese == toChinese) {
        out.hit = true;
        out.kind = ReuseKind::Repeat;
        out.text = memory.translated;
    }
    return out;
}

QString redactSecret(QString text, const QString& secret)
{
    if (!secret.isEmpty() && secret.size() >= 4) text.replace(secret, QStringLiteral("***"));
    return text;
}
}
