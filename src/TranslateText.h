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
}
