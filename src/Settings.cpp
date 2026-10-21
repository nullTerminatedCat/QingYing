#include "Settings.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QColor>
#include <QImage>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {
QSettings& store()
{
    static QSettings s(QStringLiteral("QingYing"), QStringLiteral("QingYing"));
    return s;
}
const char* kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

namespace Settings {
QKeySequence defaultHotkey() { return QKeySequence(QStringLiteral("Ctrl+Alt+A")); }

QKeySequence hotkey()
{
    const QString s = store().value("hotkey", defaultHotkey().toString()).toString();
    QKeySequence k(s);
    return k.isEmpty() ? defaultHotkey() : k;
}
void setHotkey(const QKeySequence& seq) { store().setValue("hotkey", seq.toString()); }

QString saveDir()
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/QingYing");
    return store().value("saveDir", def).toString();
}
void setSaveDir(const QString& dir) { store().setValue("saveDir", dir); }

QString filePattern() { return store().value("filePattern", QStringLiteral("QingYing_{yyyyMMdd_HHmmss}")).toString(); }
void setFilePattern(const QString& p) { store().setValue("filePattern", p); }

QString format()
{
    const QString f = store().value("format", QStringLiteral("png")).toString().toLower();
    return f == QLatin1String("jpg") ? f : QStringLiteral("png");
}
void setFormat(const QString& fmt) { store().setValue("format", fmt.toLower()); }

bool autostart()
{
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    return run.contains(QStringLiteral("QingYing"));
}
void setAutostart(bool on)
{
    QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
    if (on)
        run.setValue(QStringLiteral("QingYing"),
                     QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    else
        run.remove(QStringLiteral("QingYing"));
}

int historySize() { return qBound(0, store().value("historySize", 20).toInt(), 500); }
void setHistorySize(int n) { store().setValue("historySize", n); }
QString defaultAction()
{
    const QString a = store().value("defaultAction", QStringLiteral("copy")).toString();
    return (a == QLatin1String("save") || a == QLatin1String("pin")) ? a : QStringLiteral("copy");
}
void setDefaultAction(const QString& a) { store().setValue("defaultAction", a); }

int recordFps()
{
    const int f = store().value("recordFps", 15).toInt();
    return (f == 10 || f == 30) ? f : 15;
}
void setRecordFps(int fps) { store().setValue("recordFps", fps); }
bool recordCursor() { return store().value("recordCursor", true).toBool(); }
void setRecordCursor(bool on) { store().setValue("recordCursor", on); }

bool autoUpdate() { return store().value("autoUpdate", true).toBool(); }
void setAutoUpdate(bool on) { store().setValue("autoUpdate", on); }
QDateTime lastUpdateCheck() { return store().value("lastUpdateCheck").toDateTime(); }
void setLastUpdateCheck(const QDateTime& t) { store().setValue("lastUpdateCheck", t); }

QString theme()
{
    const QString t = store().value("theme", QStringLiteral("system")).toString();
    return (t == QLatin1String("light") || t == QLatin1String("dark")) ? t : QStringLiteral("system");
}
void setTheme(const QString& t) { store().setValue("theme", t); }

QString colorFormat() { return store().value("colorFormat", QStringLiteral("hex")).toString(); }
void setColorFormat(const QString& f) { store().setValue("colorFormat", f); }
QString formatColor(const QColor& c)
{
    const QString f = colorFormat();
    if (f == QLatin1String("hexlower")) return c.name().toLower();
    if (f == QLatin1String("rgb")) return QStringLiteral("rgb(%1, %2, %3)").arg(c.red()).arg(c.green()).arg(c.blue());
    if (f == QLatin1String("rgbplain")) return QStringLiteral("%1, %2, %3").arg(c.red()).arg(c.green()).arg(c.blue());
    return c.name().toUpper();
}

QString expandPattern(const QString& pattern)
{
    const QDateTime now = QDateTime::currentDateTime();
    QString out = pattern;
    static const QRegularExpression re(QStringLiteral("\\{([^}]*)\\}"));
    auto it = re.globalMatch(pattern);
    QList<QPair<QString, QString>> reps;
    while (it.hasNext()) {
        const auto m = it.next();
        reps.append({m.captured(0), now.toString(m.captured(1))});
    }
    for (const auto& r : reps) out.replace(r.first, r.second);
    static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|]"));
    out.replace(bad, QStringLiteral("_"));
    out = out.trimmed();
    return out.isEmpty() ? now.toString(QStringLiteral("'QingYing_'yyyyMMdd_HHmmss")) : out;
}

QString defaultFilePath()
{
    return QDir(saveDir()).filePath(expandPattern(filePattern()) + QLatin1Char('.') + format());
}

bool saveImage(const QImage& img, const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString suffix = QFileInfo(path).suffix().toLower();
    const bool jpg = suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg");
    QImage out = img;
    out.setDevicePixelRatio(1);
    if (jpg) out = out.convertToFormat(QImage::Format_RGB32);
    return out.save(path, jpg ? "JPG" : "PNG", jpg ? 95 : -1);
}
}
