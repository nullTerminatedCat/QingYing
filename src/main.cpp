#include "App.h"

#include <QApplication>
#include <QFont>
#include <windows.h>

int main(int argc, char* argv[])
{
    // 单实例：已运行时直接退出
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\QingYing.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("QingYing"));
    QApplication::setApplicationDisplayName(QStringLiteral("青影"));
    QApplication::setOrganizationName(QStringLiteral("QingYing"));
    QApplication::setApplicationVersion(QStringLiteral(QINGYING_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    QApplication::setFont(f);

    App app;
    const int rc = a.exec();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
