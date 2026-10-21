#include <QCoreApplication>
#include "App.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QThreadPool>
#include <QTranslator>
#include "Settings.h"
#include "Diagnostics.h"
#include "Language.h"
#include <windows.h>

int main(int argc, char* argv[])
{
    Diagnostics::installCrashHandler();
    // 单实例：已运行时直接退出
    // 冒烟测试使用独立互斥量，不影响用户正在运行的实例
    const bool smoke = GetEnvironmentVariableW(L"QINGYING_SMOKE_REPORT", nullptr, 0) > 0;
    HANDLE mutex = CreateMutexW(nullptr, TRUE, smoke ? L"Local\\QingYing.SmokeTest" : L"Local\\QingYing.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QingYing"));
    QApplication::setOrganizationName(QStringLiteral("QingYing"));
    Language::apply(Settings::effectiveLanguage());
    QApplication::setApplicationDisplayName(QCoreApplication::translate("QingYing", "青影"));
    QApplication::setOrganizationName(QStringLiteral("QingYing"));
    QApplication::setApplicationVersion(QStringLiteral(QINGYING_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    Diagnostics::init();
    if (qEnvironmentVariableIsSet("QINGYING_SMOKE_CRASH")) { // 自检：验证崩溃日志与 minidump
        volatile int* p = nullptr;
        *p = 1;
    }
    QObject::connect(&a, &QCoreApplication::aboutToQuit, [] { Diagnostics::log(QStringLiteral("aboutToQuit")); });
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    QApplication::setFont(f);

    Theme::init();
    App app;
    const int rc = a.exec();
    Diagnostics::log(QStringLiteral("event loop finished rc=%1").arg(rc));
    // 等待后台任务（OCR 等）结束后再析构；超时则记录并直接结束进程，避免在析构阶段崩溃
    if (!QThreadPool::globalInstance()->waitForDone(5000)) {
        Diagnostics::log(QStringLiteral("background tasks still running after 5 s; terminating"));
        if (mutex) ReleaseMutex(mutex);
        TerminateProcess(GetCurrentProcess(), UINT(rc));
    }
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
