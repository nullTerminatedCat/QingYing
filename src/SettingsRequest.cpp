#include "SettingsRequest.h"

namespace SettingsRequest {
namespace {
Handler& handler()
{
    static Handler h;
    return h;
}
}

void setHandler(Handler h) { handler() = std::move(h); }

void openAi()
{
    if (handler()) handler()();
}
}
