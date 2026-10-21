#include "History.h"
#include "Settings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace History {
QString dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/history");
}

QStringList entries()
{
    QDir d(dir());
    const QStringList names = d.entryList({QStringLiteral("*.png")}, QDir::Files, QDir::Name | QDir::Reversed);
    QStringList out;
    for (const QString& n : names) out << d.filePath(n);
    return out;
}

void prune()
{
    const QStringList all = entries();
    const int keep = Settings::historySize();
    for (int i = keep; i < all.size(); ++i) QFile::remove(all[i]);
}

void add(const QImage& image)
{
    if (Settings::historySize() <= 0 || image.isNull()) return;
    QDir().mkpath(dir());
    QImage img = image;
    img.setDevicePixelRatio(1);
    const QString name = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")) + QStringLiteral(".png");
    img.save(QDir(dir()).filePath(name), "PNG");
    prune();
}

void clear()
{
    for (const QString& f : entries()) QFile::remove(f);
}
}
