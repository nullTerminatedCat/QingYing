#include "EditRequest.h"

namespace EditRequest {
namespace {
Handler& handler()
{
    static Handler h;
    return h;
}
}
void setHandler(Handler h) { handler() = std::move(h); }
bool available() { return bool(handler()); }
void request(const QImage& image, const QPoint& pos, bool hasPosition, QWidget* source)
{
    if (handler() && !image.isNull()) handler()(image, pos, hasPosition, source);
}
}
