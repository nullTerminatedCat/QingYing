#include <QLocale>
#include "Settings.h"
#include "Logic.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include "Secret.h"

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

bool uploadEnabled() { return store().value("upload/enabled", false).toBool(); }
void setUploadEnabled(bool on) { store().setValue("upload/enabled", on); }
Uploads::Config uploadConfig()
{
    Uploads::Config c;
    c.provider = store().value("upload/provider", c.provider).toString();
    c.url = store().value("upload/url").toString();
    c.field = store().value("upload/field", c.field).toString();
    c.jsonPath = store().value("upload/jsonPath", c.jsonPath).toString();
    c.secret = Secret::unprotect(store().value("upload/secret").toString());
    return c;
}
void setUploadConfig(const Uploads::Config& c)
{
    store().setValue("upload/provider", c.provider);
    store().setValue("upload/url", c.url);
    store().setValue("upload/field", c.field);
    store().setValue("upload/jsonPath", c.jsonPath);
    store().setValue("upload/secret", Secret::protect(c.secret));
}

bool autoUpdate() { return store().value("autoUpdate", true).toBool(); }
void setAutoUpdate(bool on) { store().setValue("autoUpdate", on); }
bool welcomed() { return store().value("welcomed", false).toBool(); }
void setWelcomed(bool on) { store().setValue("welcomed", on); }
QDateTime lastUpdateCheck() { return store().value("lastUpdateCheck").toDateTime(); }
void setLastUpdateCheck(const QDateTime& t) { store().setValue("lastUpdateCheck", t); }

QString theme()
{
    const QString t = store().value("theme", QStringLiteral("system")).toString();
    return (t == QLatin1String("light") || t == QLatin1String("dark")) ? t : QStringLiteral("system");
}
void setTheme(const QString& t) { store().setValue("theme", t); }

bool gifDither() { return store().value("gifDither", false).toBool(); }
void setGifDither(bool on) { store().setValue("gifDither", on); }
int defaultTool() { return qBound(0, store().value("defaultTool", 0).toInt(), 7); }
void setDefaultTool(int t) { store().setValue("defaultTool", t); }
int defaultColor() { return qMax(0, store().value("defaultColor", 0).toInt()); }
void setDefaultColor(int i) { store().setValue("defaultColor", i); }

QString language()
{
    const QString l = store().value("language", QStringLiteral("system")).toString();
    return (l == QLatin1String("zh_CN") || l == QLatin1String("en_US")) ? l : QStringLiteral("system");
}
void setLanguage(const QString& l) { store().setValue("language", l); }
QString effectiveLanguage()
{
    QString l = language();
    if (l == QLatin1String("system")) {
        const QByteArray env = qgetenv("QINGYING_LANG");
        if (!env.isEmpty()) return QString::fromLatin1(env);
        l = QLocale::system().language() == QLocale::Chinese ? QStringLiteral("zh_CN") : QStringLiteral("en_US");
    }
    return l;
}

QString colorFormat() { return store().value("colorFormat", QStringLiteral("hex")).toString(); }
void setColorFormat(const QString& f) { store().setValue("colorFormat", f); }
QString formatColor(const QColor& c) { return Logic::formatColor(c, colorFormat()); }

QString expandPattern(const QString& pattern) { return Logic::expandPattern(pattern, QDateTime::currentDateTime()); }

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
