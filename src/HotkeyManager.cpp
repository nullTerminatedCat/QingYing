#include "HotkeyManager.h"

#include <QCoreApplication>
#include <windows.h>

HotkeyManager::HotkeyManager(QObject* parent) : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

HotkeyManager::~HotkeyManager()
{
    unregister();
    if (QCoreApplication::instance()) QCoreApplication::instance()->removeNativeEventFilter(this);
}

bool HotkeyManager::toNative(const QKeySequence& seq, unsigned& mods, unsigned& vk)
{
    if (seq.isEmpty()) return false;
    const QKeyCombination c = seq[0];
    const Qt::KeyboardModifiers m = c.keyboardModifiers();
    const int k = c.key();
    mods = 0;
    if (m & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m & Qt::AltModifier) mods |= MOD_ALT;
    if (m & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m & Qt::MetaModifier) mods |= MOD_WIN;
    if (k >= Qt::Key_A && k <= Qt::Key_Z) vk = 'A' + (k - Qt::Key_A);
    else if (k >= Qt::Key_0 && k <= Qt::Key_9) vk = '0' + (k - Qt::Key_0);
    else if (k >= Qt::Key_F1 && k <= Qt::Key_F24) vk = VK_F1 + (k - Qt::Key_F1);
    else {
        switch (k) {
        case Qt::Key_Print: vk = VK_SNAPSHOT; break;
        case Qt::Key_Space: vk = VK_SPACE; break;
        case Qt::Key_Insert: vk = VK_INSERT; break;
        case Qt::Key_Delete: vk = VK_DELETE; break;
        case Qt::Key_Home: vk = VK_HOME; break;
        case Qt::Key_End: vk = VK_END; break;
        case Qt::Key_PageUp: vk = VK_PRIOR; break;
        case Qt::Key_PageDown: vk = VK_NEXT; break;
        case Qt::Key_Pause: vk = VK_PAUSE; break;
        case Qt::Key_QuoteLeft: vk = VK_OEM_3; break;
        default: return false;
        }
    }
    return true;
}

bool HotkeyManager::setHotkey(const QKeySequence& seq)
{
    unregister();
    unsigned mods = 0, vk = 0;
    if (!toNative(seq, mods, vk)) return false;
    m_registered = RegisterHotKey(nullptr, kId, mods | MOD_NOREPEAT, vk) != FALSE;
    if (m_registered) m_seq = seq;
    return m_registered;
}

void HotkeyManager::unregister()
{
    if (m_registered) UnregisterHotKey(nullptr, kId);
    m_registered = false;
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr*)
{
    if (eventType != "windows_generic_MSG") return false;
    const MSG* msg = static_cast<const MSG*>(message);
    if (msg->message == WM_HOTKEY && int(msg->wParam) == kId) {
        emit activated();
        return true;
    }
    return false;
}
