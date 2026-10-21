#pragma once
#include <QKeySequence>
#include <QString>

class QImage;

namespace Settings {
QKeySequence defaultHotkey();
QKeySequence hotkey();
void setHotkey(const QKeySequence& seq);
QString saveDir();
void setSaveDir(const QString& dir);
QString filePattern();
void setFilePattern(const QString& pattern);
QString format(); // "png" 或 "jpg"
void setFormat(const QString& fmt);
bool autostart();
void setAutostart(bool on);
QString expandPattern(const QString& pattern);
QString defaultFilePath();
bool saveImage(const QImage& img, const QString& path);
}
