#pragma once
#include <QImage>
#include <QPoint>
#include <functional>

class QWidget;

// 请求在截图编辑器中重新标注一张图片（钉图、历史记录使用；由 App 注册处理函数）
namespace EditRequest {
// logicalTopLeft 为全局逻辑坐标；hasPosition=false 时放在光标所在屏幕中央。source 为发起编辑的钉图（可为空）
using Handler = std::function<void(const QImage& image, const QPoint& logicalTopLeft, bool hasPosition, QWidget* source)>;
void setHandler(Handler h);
bool available();
void request(const QImage& image, const QPoint& logicalTopLeft, bool hasPosition, QWidget* source);
}
