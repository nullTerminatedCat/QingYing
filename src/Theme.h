#pragma once
#include <QObject>
#include <QString>

class QWidget;

// 主题：浅色 / 深色 / 跟随系统。强调色始终为 #4F6BFF
namespace Theme {
void init();             // 读取设置并监听系统主题变化
void apply();            // 重新应用当前设置
bool isDark();
bool systemIsDark();     // HKCU\...\Personalize\AppsUseLightTheme
QString themed(const QString& lightSheet);
void setSheet(QWidget* w, const QString& lightSheet); // 设置样式表并在主题切换时自动刷新
QObject* notifier();     // 发出 destroyed 以外的信号：changed()
}

class ThemeNotifier : public QObject {
    Q_OBJECT
signals:
    void changed();
};
