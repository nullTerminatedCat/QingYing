#pragma once
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVersionNumber>

// 通过 GitHub Releases 检查更新；安装版下载 setup.exe 并校验 SHA-256，免安装版打开发布页
class Updater : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kLatestApi = "https://api.github.com/repos/nullTerminatedCat/QingYing/releases/latest";
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
    QNetworkRequest request(const QUrl& url) const;
    QNetworkAccessManager m_net;
    bool m_busy = false;
};
