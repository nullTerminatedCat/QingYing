#pragma once
#include <QByteArray>
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

enum class ReuseKind { None, Already, RoundTrip, Repeat };

// 最近一次真正发出的翻译，用来省掉「译出去再立刻译回来」和重复点击。
struct TranslationMemory {
    bool valid = false;
    bool toChinese = false;
    QString source;
    QString translated;
};

struct Reuse {
    bool hit = false;
    ReuseKind kind = ReuseKind::None;
    QString text;
};

// hit 时 text 可直接显示，不必再请求 AI 或 MyMemory。
Reuse reuseTranslation(const QString& text, bool toChinese, const TranslationMemory& memory);
}
