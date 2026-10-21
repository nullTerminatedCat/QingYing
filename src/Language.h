#pragma once
#include <QString>

// 界面语言：加载 / 切换翻译（无需重启）。已打开的窗口由调用方重建以刷新文字
namespace Language {
void apply(const QString& lang); // zh_CN / en_US
QString current();
}
