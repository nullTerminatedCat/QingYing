#pragma once
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
    TranslateText::ReuseKind local = TranslateText::ReuseKind::None;
};

// 按文本是否以中日韩为主，建议译成英文或中文
Target suggestTarget(const QString& text);

// 异步翻译；done 在主线程调用。空文本或未启用时直接失败提示。
// 已是目标语言，或能用上一次结果往返还原时，同步回调且不访问网络。
void translate(const QString& text, Target target, std::function<void(Result)> done);
// 识别面板换了新文本时丢掉往返缓存
void forgetRecall();
}