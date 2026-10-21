#pragma once
#include <QImage>
#include <QList>
#include <QNetworkRequest>
#include <QString>
#include <functional>
#include <memory>

class QHttpMultiPart;

// 上传插件接口：每个提供者负责构造请求和解析返回的图片链接
class UploadProvider {
public:
    virtual ~UploadProvider() = default;
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual bool isConfigurable() const { return false; } // 自定义 URL / 字段 / JSON 路径
    virtual QString secretHint() const = 0;
    virtual QNetworkRequest request() const = 0;
    virtual QHttpMultiPart* body(const QByteArray& png) const = 0;
    virtual QString parseUrl(const QByteArray& reply, QString* error) const = 0;
};

namespace Uploads {
struct Config {
    QString provider = QStringLiteral("custom"); // custom / smms / imgur
    QString url;
    QString field = QStringLiteral("file");
    QString jsonPath = QStringLiteral("url");
    QString secret; // 明文仅存在于内存
};
QStringList providerIds();
std::unique_ptr<UploadProvider> create(const Config& cfg);
// 按 a.b.0.c 形式的路径从 JSON 中取字符串
QString jsonValue(const QByteArray& json, const QString& path);
// 异步上传；done(url, error)
void upload(const QImage& image, const Config& cfg, std::function<void(QString, QString)> done);
// 使用当前设置上传，成功后复制链接并提示
void uploadWithSettings(const QImage& image);
}
