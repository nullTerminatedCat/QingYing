#pragma once
#include <functional>

// 从结果面板打开设置里的 AI 接口。由 App 注册处理函数。
namespace SettingsRequest {
using Handler = std::function<void()>;
void setHandler(Handler h);
void openAi();
}
