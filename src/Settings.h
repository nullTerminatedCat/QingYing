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
QKeySequence defaultRecordHotkey();
QKeySequence recordHotkey();
void setRecordHotkey(const QKeySequence& seq);
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
QString colorFormat(); // hex / hexlower / hexa / rgb / rgba / rgbplain / hsl / hsv / bgr
void setColorFormat(const QString& f);
QString formatColor(const QColor& c);
int recordFps();
void setRecordFps(int fps);
bool recordCursor();
void setRecordCursor(bool on);
bool recordMic(); // MP4 录制麦克风，默认关；GIF 忽略
void setRecordMic(bool on);
QString theme(); // system / light / dark
void setTheme(const QString& t);
bool gifDither();
void setGifDither(bool on);
int defaultTool(); // 0 无，1-7 对应 Tool 枚举
void setDefaultTool(int t);
int defaultColor(); // 调色板下标
void setDefaultColor(int i);
QString language(); // system / zh_CN / en_US
void setLanguage(const QString& l);
QString effectiveLanguage(); // zh_CN 或 en_US
bool welcomed(); // 是否已显示首次运行提示
void setWelcomed(bool on);
bool autoUpdate();
void setAutoUpdate(bool on);
QDateTime lastUpdateCheck();
void setLastUpdateCheck(const QDateTime& t);
bool uploadEnabled();
void setUploadEnabled(bool on);
bool translateEnabled(); // OCR 结果联网翻译，默认开
void setTranslateEnabled(bool on);
QString ocrEngine(); // rapid / system，默认 rapid
void setOcrEngine(const QString& engine);
bool aiTranslateEnabled();
void setAiTranslateEnabled(bool on);
QString aiBaseUrl();
void setAiBaseUrl(const QString& url);
QString aiModel();
void setAiModel(const QString& model);
QString aiApiKey(); // DPAPI 解密后的明文，仅存在于内存
void setAiApiKey(const QString& key);
bool memoryFallback(); // AI 未配置或失败时改用 MyMemory，默认开
void setMemoryFallback(bool on);
bool aiTranslateReady(); // 已启用且地址、密钥、模型都可用
Uploads::Config uploadConfig(); // 密钥经 DPAPI 解密
void setUploadConfig(const Uploads::Config& c); // 密钥经 DPAPI 加密后保存
QString expandPattern(const QString& pattern);
QString defaultFilePath();
bool saveImage(const QImage& img, const QString& path);
}
