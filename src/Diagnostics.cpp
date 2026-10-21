#include "Diagnostics.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>
#include <cstdlib>
#include <exception>
#include <mutex>

#include <windows.h>
#include <dbghelp.h>

namespace {
std::mutex g_mutex;
QString g_logPath;
wchar_t g_dumpDir[MAX_PATH] = {};
wchar_t g_logW[MAX_PATH] = {};
QtMessageHandler g_prev = nullptr;

// 崩溃路径：只用 Win32 API，不分配 Qt 内存
void rawLog(const char* text)
{
    if (!g_logW[0]) return;
    HANDLE h = CreateFileW(g_logW, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD n = 0;
    WriteFile(h, text, DWORD(strlen(text)), &n, nullptr);
    CloseHandle(h);
}

void writeDump(EXCEPTION_POINTERS* ep)
{
    if (!g_dumpDir[0]) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t path[MAX_PATH];
    swprintf_s(path, L"%s\\crash_%04d%02d%02d_%02d%02d%02d_%lu.dmp", g_dumpDir, st.wYear, st.wMonth, st.wDay, st.wHour,
               st.wMinute, st.wSecond, GetCurrentProcessId());
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                      MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo), ep ? &mei : nullptr,
                      nullptr, nullptr);
    CloseHandle(f);
}

LONG WINAPI onCrash(EXCEPTION_POINTERS* ep)
{
    char buf[256];
    sprintf_s(buf, "CRASH exception=0x%08lX address=%p thread=%lu\r\n", ep->ExceptionRecord->ExceptionCode,
              ep->ExceptionRecord->ExceptionAddress, GetCurrentThreadId());
    rawLog(buf);
    writeDump(ep);
    return EXCEPTION_EXECUTE_HANDLER;
}

void onTerminate()
{
    rawLog("CRASH std::terminate\r\n");
    writeDump(nullptr);
    std::abort();
}

void onPureCall()
{
    rawLog("CRASH pure virtual call\r\n");
    writeDump(nullptr);
    std::abort();
}

void onMessage(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    static const char* names[] = {"debug", "warning", "critical", "fatal", "info"};
    Diagnostics::log(QStringLiteral("[%1] %2").arg(QLatin1String(names[qBound(0, int(type), 4)]), msg));
    if (g_prev) g_prev(type, ctx, msg);
}

void onExit() { rawLog("process exit (atexit)\r\n"); }
}

namespace Diagnostics {
void installCrashHandler()
{
    SetUnhandledExceptionFilter(onCrash);
    std::set_terminate(onTerminate);
    _set_purecall_handler(onPureCall);
    std::atexit(onExit);
}

QString logDir() { return QFileInfo(g_logPath).absolutePath(); }

void init()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
    QDir().mkpath(dir);
    g_logPath = dir + QStringLiteral("/qingying.log");
    if (QFileInfo(g_logPath).size() > 1024 * 1024) { // 简单轮转：保留上一份
        QFile::remove(g_logPath + QStringLiteral(".1"));
        QFile::rename(g_logPath, g_logPath + QStringLiteral(".1"));
    }
    QDir::toNativeSeparators(dir).toWCharArray(g_dumpDir);
    QDir::toNativeSeparators(g_logPath).toWCharArray(g_logW);
    // 只保留最近 5 个转储
    QDir d(dir);
    const QStringList dumps = d.entryList({QStringLiteral("crash_*.dmp")}, QDir::Files, QDir::Time);
    for (int i = 5; i < dumps.size(); ++i) d.remove(dumps[i]);
    g_prev = qInstallMessageHandler(onMessage);
    log(QStringLiteral("start version=%1 pid=%2 args=%3")
            .arg(QCoreApplication::applicationVersion())
            .arg(GetCurrentProcessId())
            .arg(QCoreApplication::arguments().join(QLatin1Char(' '))));
}

void log(const QString& line)
{
    if (g_logPath.isEmpty()) return;
    std::lock_guard lock(g_mutex);
    QFile f(g_logPath);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz ")) + line + QLatin1Char('\n')).toUtf8());
}
}
