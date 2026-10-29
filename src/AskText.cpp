#include "AskText.h"

#include <QCoreApplication>

namespace AskText {
namespace {
QString norm(QString s)
{
    s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    s.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return s.trimmed();
}

QString basePrompt(Mode mode)
{
    switch (mode) {
    case Mode::Ask:
        return QStringLiteral(
            "你是截图助手。用户会给你一张截图里识别出的文字，以及一个问题。"
            "只根据这些文字回答，不要编造图里没有的信息。文字不够时直接说明。"
            "用与问题相同的语言简洁回答。不要加标题或前后说明。");
    case Mode::Polite:
        return QStringLiteral(
            "把用户给出的文案改得更礼貌、更客气，保留原意、专有名词和关键数字。"
            "使用与原文相同的语言。只输出改写后的文案，不要解释。");
    case Mode::Shorter:
        return QStringLiteral(
            "把用户给出的文案改得更短，保留原意和关键信息，删掉客套和重复。"
            "使用与原文相同的语言。只输出改写后的文案，不要解释。");
    case Mode::Formal:
        return QStringLiteral(
            "把用户给出的文案改得更正式，适合工作场合，保留原意、专有名词和关键数字。"
            "使用与原文相同的语言。只输出改写后的文案，不要解释。");
    case Mode::Polish:
        return QStringLiteral(
            "润色用户给出的文案：修正错别字和语病，让句子更通顺，不改变原意。"
            "使用与原文相同的语言。只输出润色后的文案，不要解释。");
    }
    return {};
}
}

QList<Mode> rewriteModes()
{
    return {Mode::Polite, Mode::Shorter, Mode::Formal, Mode::Polish};
}

QString modeTitle(Mode mode)
{
    switch (mode) {
    case Mode::Ask: return QCoreApplication::translate("QingYing", "截图秒答");
    case Mode::Polite: return QCoreApplication::translate("QingYing", "更礼貌");
    case Mode::Shorter: return QCoreApplication::translate("QingYing", "更短");
    case Mode::Formal: return QCoreApplication::translate("QingYing", "更正式");
    case Mode::Polish: return QCoreApplication::translate("QingYing", "润色");
    }
    return {};
}

QString shortcutHint(Mode mode)
{
    if (mode == Mode::Ask) return QStringLiteral("Ctrl+Enter");
    const QList<Mode> modes = rewriteModes();
    for (int i = 0; i < modes.size(); ++i)
        if (modes.at(i) == mode) return QStringLiteral("Ctrl+%1").arg(i + 1);
    return {};
}

QString systemPrompt(Mode mode, bool truncated)
{
    QString text = basePrompt(mode);
    if (truncated)
        text += QStringLiteral(" 给出的文字已截断，只处理这一部分。");
    return text;
}

QString userContent(Mode mode, const QString& text, const QString& question, bool* truncated)
{
    QString body = norm(text);
    bool cut = false;
    if (body.size() > kMaxChars) {
        body.truncate(kMaxChars);
        cut = true;
    }
    if (truncated) *truncated = cut;
    if (mode != Mode::Ask) return body;
    return QStringLiteral("下面是一张截图里识别出的文字，不是用户手打的全文：\n%1\n\n问题：%2")
        .arg(body, norm(question));
}

bool textEmpty(const QString& text) { return norm(text).isEmpty(); }
bool questionEmpty(const QString& question) { return norm(question).isEmpty(); }

QString cacheKey(Mode mode, const QString& text, const QString& question)
{
    const QString q = mode == Mode::Ask ? norm(question) : QString();
    return QString::number(int(mode)) + QLatin1Char('\n') + norm(text) + QLatin1Char('\n') + q;
}

bool lookup(const Cache& cache, const QString& key, qint64 nowMs, QString* answer)
{
    for (int i = cache.size() - 1; i >= 0; --i) {
        const Slot& slot = cache.at(i);
        if (slot.key != key || slot.answer.isEmpty()) continue;
        if (nowMs < slot.atMs || nowMs - slot.atMs > kCacheMs) continue;
        if (answer) *answer = slot.answer;
        return true;
    }
    return false;
}

void remember(Cache& cache, const QString& key, const QString& answer, qint64 nowMs)
{
    if (key.isEmpty() || answer.isEmpty()) return;
    Cache kept;
    for (const Slot& slot : cache) {
        if (slot.key == key) continue;
        if (nowMs < slot.atMs || nowMs - slot.atMs > kCacheMs) continue;
        kept.append(slot);
    }
    while (kept.size() >= kCacheCap) kept.removeFirst();
    kept.append(Slot{key, answer, nowMs});
    cache = kept;
}
}
