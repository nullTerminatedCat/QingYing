#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVersionNumber>

class QProgressDialog;

// 通过 GitHub Releases 检查更新；安装版下载 setup.exe 并校验 SHA-256，免安装版打开发布页。
// 手动检查：进度对话框 + 结果一律用消息框（不依赖易被系统吞掉的托盘气泡）。
// API 限流/失败时回退到 github.com/releases/latest 的跳转解析版本。
class Updater : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kLatestApi = "https://api.github.com/repos/nullTerminatedCat/QingYing/releases/latest";
    static constexpr const char* kLatestPage = "https://github.com/nullTerminatedCat/QingYing/releases/latest";
    explicit Updater(QObject* parent = nullptr);
    void check(bool manual);
    void maybeAutoCheck();
    static bool isPortable();

    struct Release {
        QString tag;
        QVersionNumber version;
        QString notes;
        QUrl page;
        QUrl setupUrl;
        QString setupName;
        QUrl sumsUrl;
    };
    static bool parse(const QByteArray& json, Release* out);

private:
    void offer(const Release& r);
    void download(const Release& r);
    void finishManualBusy();
    void reportManual(const QString& title, const QString& text, bool warn);
    void handleApiResult(const QByteArray& body, int http, int err, const QString& errStr, bool manual);
    void tryHtmlFallback(bool manual);
    void applyRelease(const Release& r, bool manual);
    QNetworkRequest request(const QUrl& url) const;
    QNetworkAccessManager m_net;
    bool m_busy = false;
    QProgressDialog* m_busyDlg = nullptr;
};
