#include <QCoreApplication>
#include "App.h"
#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QTranslator>
#include "Settings.h"
#include <windows.h>

int main(int argc, char* argv[])
{
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
    // 源码文字为简体中文；其他语言加载内置 .qm（语言切换需重启生效）
    QTranslator translator;
    const QString lang = Settings::effectiveLanguage();
    if (lang != QLatin1String("zh_CN") && translator.load(QStringLiteral(":/i18n/qingying_%1.qm").arg(lang)))
        QApplication::installTranslator(&translator);
    QApplication::setApplicationDisplayName(QCoreApplication::translate("QingYing", "青影"));
    QApplication::setOrganizationName(QStringLiteral("QingYing"));
    QApplication::setApplicationVersion(QStringLiteral(QINGYING_VERSION));
    QApplication::setQuitOnLastWindowClosed(false);
    QFont f(QStringLiteral("Microsoft YaHei UI"));
    f.setPixelSize(13);
    QApplication::setFont(f);

    Theme::init();
    App app;
    const int rc = a.exec();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return rc;
}
