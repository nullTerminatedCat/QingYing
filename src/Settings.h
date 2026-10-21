#pragma once
#include <QKeySequence>
#include <QString>
#include "Uploader.h"

class QImage;
class QColor;
class QDateTime;

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
QString theme(); // system / light / dark
void setTheme(const QString& t);
bool autoUpdate();
void setAutoUpdate(bool on);
QDateTime lastUpdateCheck();
void setLastUpdateCheck(const QDateTime& t);
bool uploadEnabled();
void setUploadEnabled(bool on);
Uploads::Config uploadConfig(); // 密钥经 DPAPI 解密
void setUploadConfig(const Uploads::Config& c); // 密钥经 DPAPI 加密后保存
QString expandPattern(const QString& pattern);
QString defaultFilePath();
bool saveImage(const QImage& img, const QString& path);
}
