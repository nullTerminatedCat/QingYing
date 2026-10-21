#pragma once
#include <QString>

// 诊断：日志文件（qInstallMessageHandler）、未处理异常写 minidump、记录退出原因。
// 日志与转储位于 %LOCALAPPDATA%\QingYing\QingYing\logs
namespace Diagnostics {
void installCrashHandler();      // 尽早调用（QApplication 之前也可）
void init();                     // QApplication 创建且设置应用名之后调用
void log(const QString& line);   // 写一行到日志
QString logDir();
}
