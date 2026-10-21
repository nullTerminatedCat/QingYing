#include "WindowDetector.h"

#include <algorithm>
#include <dwmapi.h>
#include <windows.h>

namespace {
QRect toQRect(const RECT& r) { return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top); }

bool isCloaked(HWND h)
{
    int cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked;
}

BOOL CALLBACK childProc(HWND h, LPARAM lp)
{
    auto* list = reinterpret_cast<QList<QRect>*>(lp);
    if (!IsWindowVisible(h)) return TRUE;
    RECT r;
    if (GetWindowRect(h, &r) && r.right - r.left > 4 && r.bottom - r.top > 4) list->append(toQRect(r));
    return TRUE;
}

struct Ctx {
    QList<QRect>* tops;
    QList<QList<QRect>>* kids;
};

BOOL CALLBACK topProc(HWND h, LPARAM lp)
{
    auto* ctx = reinterpret_cast<Ctx*>(lp);
    if (!IsWindowVisible(h) || IsIconic(h) || isCloaked(h)) return TRUE;
    const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
    if (ex & WS_EX_TRANSPARENT) return TRUE;
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) GetWindowRect(h, &r);
    if (r.right - r.left < 8 || r.bottom - r.top < 8) return TRUE;
    ctx->tops->append(toQRect(r));
    QList<QRect> children;
    EnumChildWindows(h, childProc, reinterpret_cast<LPARAM>(&children));
    ctx->kids->append(children);
    return TRUE;
}
}

void WindowDetector::snapshot()
{
    m_windows.clear();
    QList<QRect> tops;
    QList<QList<QRect>> kids;
    Ctx ctx{&tops, &kids};
    EnumWindows(topProc, reinterpret_cast<LPARAM>(&ctx));
    for (int i = 0; i < tops.size(); ++i) {
        Entry e{tops[i], kids[i]};
        const qint64 area = qint64(e.rect.width()) * e.rect.height();
        // 去掉几乎铺满窗口的面板，避免永远选不中整窗
        e.children.erase(std::remove_if(e.children.begin(), e.children.end(), [&](const QRect& c) {
                             return qint64(c.width()) * c.height() >= area * 85 / 100;
                         }), e.children.end());
        std::sort(e.children.begin(), e.children.end(), [](const QRect& a, const QRect& b) {
            return qint64(a.width()) * a.height() < qint64(b.width()) * b.height();
        });
        m_windows.append(e);
    }
}

QRect WindowDetector::hitTest(const QPoint& p, bool controls) const
{
    for (const Entry& e : m_windows) {
        if (!e.rect.contains(p)) continue;
        if (controls)
            for (const QRect& c : e.children)
                if (c.contains(p)) return c.intersected(e.rect);
        return e.rect;
    }
    return {};
}
