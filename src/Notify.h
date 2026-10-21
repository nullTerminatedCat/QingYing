#pragma once
#include <QString>

// 托盘气泡通知（由 App 注册托盘图标）
namespace Notify {
void info(const QString& title, const QString& text);
void warn(const QString& title, const QString& text);
}
