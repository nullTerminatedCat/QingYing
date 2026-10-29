#pragma once
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
    QString engine; // "ai" 或 "mymemory"
    bool fellBack = false; // AI 失败或未配置后改用了 MyMemory
};

// 按文本是否以中日韩为主，建议译成英文或中文
Target suggestTarget(const QString& text);

// 异步翻译；done 在主线程调用。空文本或未启用时直接失败提示。
void translate(const QString& text, Target target, std::function<void(Result)> done);
}