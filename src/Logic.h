#pragma once
#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QString>

// 不依赖界面与设置的纯逻辑，便于单元测试（tests/）
namespace Logic {
// 长截图：返回 cur 相对 prev 向上滚动的像素数；无法可靠匹配返回 -1，完全相同返回 0
int findShift(const QImage& prev, const QImage& cur);
// 文件名规则：{} 内按 QDateTime 格式展开，并替换 Windows 文件名非法字符
QString expandPattern(const QString& pattern, const QDateTime& now);
// 取色格式：hex / hexlower / rgb / rgbplain
QString formatColor(const QColor& c, const QString& format);
// 语义化版本比较：tag 可带 v 前缀；预发布（-beta 等）低于同号正式版
bool isNewerVersion(const QString& remoteTag, const QString& current);
// SHA256SUMS 文本中查找文件对应的哈希（小写）；找不到返回空
QString expectedSha256(const QString& sums, const QString& fileName);
QString sha256Hex(const QByteArray& data);
// 按 a.b.0.c 路径取 JSON 字符串值
QString jsonValue(const QByteArray& json, const QString& path);
}
