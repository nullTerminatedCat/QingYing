#pragma once
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

// OpenAI 兼容 Chat Completions 的纯文本处理，便于单测；不发起网络请求。
namespace TranslateText {
QUrl chatCompletionsUrl(const QString& baseUrl);
QString systemPrompt(bool toChinese);
QStringList chunkForModel(const QString& text, int limit = 3500);
// 解析 choices[0].message.content；失败时 error 为可读原因（不含密钥）
QString parseChatCompletion(const QByteArray& body, QString* error);
QString redactSecret(QString text, const QString& secret);

// 源文已经是目标语言（或没有需要翻译的字母）时为 true。空文本为 false。
// 中英都有、或含日文假名等其他字母时为 false，仍应走翻译。
bool alreadyInTargetLanguage(const QString& text, bool toChinese);
// 空白，或去掉网址、邮箱后不再有字母（数字、标点、纯链接）。空文本也算。
bool lacksProse(const QString& text);

enum class ReuseKind { None, Already, RoundTrip, Repeat, NoProse };

// 同一次识别里记住的译文。换了新文本应清空。不做时间过期，避免过期后又打一次相同请求。
struct CacheEntry {
    QString source;
    bool toChinese = false;
    QString translated;
};

inline constexpr int kSessionCache = 8;

struct TranslationMemory {
    QList<CacheEntry> entries; // 最近的在末尾
};

struct Reuse {
    bool hit = false;
    ReuseKind kind = ReuseKind::None;
    QString text;
};

void rememberTranslation(TranslationMemory& memory, const QString& source, bool toChinese, const QString& translated);
// 当前文本是否就是某次缓存的译文（用于自动「翻译」按钮不再往回翻）。
bool isCachedOutput(const QString& text, const TranslationMemory& memory);
// hit 时 text 可直接显示，不必再请求 AI 或 MyMemory。
Reuse reuseTranslation(const QString& text, bool toChinese, const TranslationMemory& memory);
}
