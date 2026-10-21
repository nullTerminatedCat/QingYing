#include "Theme.h"
#include "Settings.h"
#include "Style.h"

#include <QApplication>
#include <QSettings>
#include <QStyleHints>
#include <QWidget>

namespace {
bool g_dark = false;
const char* kRawProp = "qyLightSheet";
ThemeNotifier* notifierObj()
{
    static auto* n = new ThemeNotifier;
    return n;
}

void setColors(bool dark)
{
    using namespace Style;
    if (dark) {
        Text = QColor(0xF2, 0xF2, 0xF7);
        Text2 = QColor(0xA1, 0xA1, 0xA6);
        Text3 = QColor(0x7C, 0x7C, 0x82);
        Line = QColor(255, 255, 255, 20);
        Glass = QColor(36, 36, 41, 240);
        Hover = QColor(255, 255, 255, 22);
        Separator = QColor(255, 255, 255, 34);
        Border = QColor(255, 255, 255, 26);
    } else {
        Text = QColor(0x1C, 0x1C, 0x1E);
        Text2 = QColor(0x6E, 0x6E, 0x73);
        Text3 = QColor(0xA1, 0xA1, 0xA6);
        Line = QColor(0, 0, 0, 15);
        Glass = QColor(255, 255, 255, 236);
        Hover = QColor(0, 0, 0, 13);
        Separator = QColor(0, 0, 0, 22);
        Border = QColor(0, 0, 0, 18);
    }
}
}

namespace Theme {
QObject* notifier() { return notifierObj(); }

bool systemIsDark()
{
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                  QSettings::NativeFormat);
    return reg.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
}

bool isDark() { return g_dark; }

QString themed(const QString& s)
{
    if (!g_dark) return s;
    QString out = s;
    const std::pair<const char*, const char*> map[] = {
        {"#F6F7FB", "#1C1D22"}, {"#FFFFFF", "#2A2B31"}, {"#1C1C1E", "#F2F2F7"}, {"#6E6E73", "#A1A1A6"},
        {"#F0F2FA", "#363740"}, {"#D5D7DE", "#4A4B52"}, {"rgba(0,0,0,", "rgba(255,255,255,"},
    };
    for (const auto& [a, b] : map) out.replace(QLatin1String(a), QLatin1String(b), Qt::CaseInsensitive);
    return out;
}

void setSheet(QWidget* w, const QString& sheet)
{
    if (!w) return;
    w->setProperty(kRawProp, sheet);
    w->setStyleSheet(themed(sheet));
}

void apply()
{
    // QINGYING_THEME 仅用于测试截图（light / dark），覆盖设置
    const QString forced = qEnvironmentVariable("QINGYING_THEME");
    const QString mode = (forced == QLatin1String("light") || forced == QLatin1String("dark")) ? forced : Settings::theme();
    auto* hints = QGuiApplication::styleHints();
    g_dark = mode == QLatin1String("dark") || (mode == QLatin1String("system") && systemIsDark());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    hints->setColorScheme(mode == QLatin1String("dark")    ? Qt::ColorScheme::Dark
                          : mode == QLatin1String("light") ? Qt::ColorScheme::Light
                                                           : Qt::ColorScheme::Unknown);
#endif
    setColors(g_dark);
    for (QWidget* w : QApplication::allWidgets()) {
        const QVariant raw = w->property(kRawProp);
        if (raw.isValid()) w->setStyleSheet(themed(raw.toString()));
        w->update();
    }
    emit notifierObj()->changed();
}

void init()
{
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, notifierObj(), [] {
        if (Settings::theme() == QLatin1String("system")) {
            const bool dark = systemIsDark();
            if (dark != g_dark) apply();
        }
    });
    apply();
}
}
