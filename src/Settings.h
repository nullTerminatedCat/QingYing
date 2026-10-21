#pragma once
#include <QKeySequence>
#include <QString>

class QImage;
class QColor;

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
int historySize();
void setHistorySize(int n);
QString defaultAction(); // "copy" / "save" / "pin"
void setDefaultAction(const QString& a);
QString colorFormat(); // hex / hexlower / rgb / rgbplain
void setColorFormat(const QString& f);
QString formatColor(const QColor& c);
int recordFps();
void setRecordFps(int fps);
bool recordCursor();
void setRecordCursor(bool on);
QString expandPattern(const QString& pattern);
QString defaultFilePath();
bool saveImage(const QImage& img, const QString& path);
}
