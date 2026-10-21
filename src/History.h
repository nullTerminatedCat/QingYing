#pragma once
#include <QImage>
#include <QStringList>

// 截图历史：PNG 存于 AppData/history，按设置保留最近 N 张
namespace History {
QString dir();
void add(const QImage& image);
QStringList entries(); // 最新在前
void clear();
void prune();
}
