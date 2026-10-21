#include "Language.h"

#include <QCoreApplication>
#include <QTranslator>

namespace Language {
namespace {
QTranslator* g_app = nullptr;
QTranslator* g_qt = nullptr;
QString g_current = QStringLiteral("zh_CN");
}

void apply(const QString& lang)
{
    if (!g_app) {
        g_app = new QTranslator(qApp);
        g_qt = new QTranslator(qApp);
    }
    QCoreApplication::removeTranslator(g_app);
    QCoreApplication::removeTranslator(g_qt);
    // 源码文字为简体中文；其他语言加载内置 .qm。Qt 自带对话框英文为默认文字
    if (lang != QLatin1String("zh_CN") && g_app->load(QStringLiteral(":/i18n/qingying_%1.qm").arg(lang)))
        QCoreApplication::installTranslator(g_app);
    if (lang != QLatin1String("en_US") && g_qt->load(QStringLiteral(":/i18n/qtbase_%1.qm").arg(lang)))
        QCoreApplication::installTranslator(g_qt);
    g_current = lang;
}

QString current() { return g_current; }
}
