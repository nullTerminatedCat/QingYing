#include "ScreenGrabber.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtGui/qscreen_platform.h>
#include <windows.h>

namespace ScreenGrabber {
QImage grab(const QRect& r)
{
    if (r.isEmpty()) return {};
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = r.width();
    bi.bmiHeader.biHeight = -r.height();
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage out;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        BitBlt(mem, 0, 0, r.width(), r.height(), screen, r.x(), r.y(), SRCCOPY | CAPTUREBLT);
        GdiFlush();
        QImage view(static_cast<const uchar*>(bits), r.width(), r.height(), r.width() * 4, QImage::Format_RGB32);
        out = view.copy();
        SelectObject(mem, old);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return out;
}

QRect monitorRect(QScreen* screen)
{
    if (auto* ni = screen->nativeInterface<QNativeInterface::QWindowsScreen>()) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(ni->handle(), &mi))
            return QRect(mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top);
    }
    const QRect g = screen->geometry();
    const qreal d = screen->devicePixelRatio();
    return QRect(g.topLeft(), (QSizeF(g.size()) * d).toSize());
}
}
