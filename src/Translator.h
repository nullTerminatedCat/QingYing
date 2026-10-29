#pragma once
#include "AskText.h"
#include "TranslateText.h"

#include <QString>
#include <functional>

// OCR 结果联网翻译：MyMemory 公开接口，无需密钥；可在设置中关闭
namespace Translator {
enum class Target { Chinese, English };

struct Result {
    bool ok = false;
    QString text;
    QString error;
    QString pair; // MyMemory 的 langpair，如 en|zh-CN；AI 时为空
    QString engine; // "ai"、"mymemory" 或 "local"（未联网）
    bool fellBack = false; // AI 失败或未配置后改用了 MyMemory
    bool fromCache = false; // 秒答 / 改文案命中短时缓存，未再请求
    TranslateText::ReuseKind local = TranslateText::ReuseKind::None;
};

// 按文本是否以中日韩为主，建议译成英文或中文
Target suggestTarget(const QString& text);

// 异步翻译；done 在主线程调用。未启用时直接失败提示。
// 已是目标语言、没有正文、或能用本次识别里的缓存往返/复用时，同步回调且不访问网络。
void translate(const QString& text, Target target, std::function<void(Result)> done);
// 识别面板换了新文本时丢掉本次缓存
void forgetRecall();
// 当前文字是不是本次缓存里的译文。自动「翻译」用来避免立刻翻回去。
bool textIsCachedTranslation(const QString& text);
// 断开并中止进行中的请求，不再发后续分块。已送到服务端的单次调用可能仍会计费。
void cancelInFlight();

// 截图秒答 / 改文案。共用翻译里的接口地址、密钥和模型，不走 MyMemory。
// 没有正文、提问时问题为空、或两分钟内相同提示与正文，同步回调且不访问网络。
// question 仅 Mode::Ask 使用。
void assist(AskText::Mode mode, const QString& text, const QString& question, std::function<void(Result)> done);
}