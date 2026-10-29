#pragma once
#include <QList>
#include <QString>

// 截图秒答 / 改文案的提示词与短时缓存。不发起网络请求。
namespace AskText {
enum class Mode { Ask, Polite, Shorter, Formal, Polish };

inline constexpr int kMaxChars = 4000;
inline constexpr qint64 kCacheMs = 120000; // 相同提示与正文两分钟内不重复请求
inline constexpr int kCacheCap = 8;

struct Slot {
    QString key;
    QString answer;
    qint64 atMs = 0;
};

using Cache = QList<Slot>;

QList<Mode> rewriteModes();
QString modeTitle(Mode mode);
// 发给模型的系统提示。truncated 时补一句「只处理截断后的部分」。不随界面语言改变。
QString systemPrompt(Mode mode, bool truncated = false);
// 用户消息。提问会附上「来自截图」的说明；改写只发送正文。过长时截断并置 truncated。
QString userContent(Mode mode, const QString& text, const QString& question, bool* truncated = nullptr);
bool textEmpty(const QString& text);
bool questionEmpty(const QString& question);

QString cacheKey(Mode mode, const QString& text, const QString& question);
// 命中且未过期时写入 answer。过期项留到 remember 时清掉。
bool lookup(const Cache& cache, const QString& key, qint64 nowMs, QString* answer);
void remember(Cache& cache, const QString& key, const QString& answer, qint64 nowMs);
}
