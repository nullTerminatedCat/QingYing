#include <QCoreApplication>
#include "Uploader.h"
#include "Logic.h"
#include "Notify.h"
#include "Settings.h"

#include <QBuffer>
#include <QClipboard>
#include <QGuiApplication>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

namespace {
QNetworkAccessManager* net()
{
    static auto* n = new QNetworkAccessManager(qApp);
    return n;
}

class HttpProvider : public UploadProvider {
public:
    HttpProvider(QString id, QString name, QUrl url, QString field, QString jsonPath, QList<QPair<QByteArray, QByteArray>> headers,
                 bool configurable, QString hint)
        : m_id(std::move(id)), m_name(std::move(name)), m_url(std::move(url)), m_field(std::move(field)),
          m_path(std::move(jsonPath)), m_headers(std::move(headers)), m_conf(configurable), m_hint(std::move(hint))
    {
    }
    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    bool isConfigurable() const override { return m_conf; }
    QString secretHint() const override { return m_hint; }
    QNetworkRequest request() const override
    {
        QNetworkRequest req(m_url);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QingYing/%1").arg(QCoreApplication::applicationVersion()));
        req.setTransferTimeout(60000);
        for (const auto& h : m_headers) req.setRawHeader(h.first, h.second);
        return req;
    }
    QHttpMultiPart* body(const QByteArray& png) const override
    {
        auto* mp = new QHttpMultiPart(QHttpMultiPart::FormDataType);
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("image/png"));
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"%1\"; filename=\"qingying.png\"").arg(m_field));
        part.setBody(png);
        mp->append(part);
        return mp;
    }
    QString parseUrl(const QByteArray& reply, QString* error) const override
    {
        const QString url = Uploads::jsonValue(reply, m_path);
        if (url.isEmpty() && error) *error = QCoreApplication::translate("QingYing", "返回内容中找不到 %1：%2").arg(m_path, QString::fromUtf8(reply.left(200)));
        return url;
    }

private:
    QString m_id, m_name;
    QUrl m_url;
    QString m_field, m_path;
    QList<QPair<QByteArray, QByteArray>> m_headers;
    bool m_conf;
    QString m_hint;
};

QList<QPair<QByteArray, QByteArray>> parseHeaders(const QString& text)
{
    QList<QPair<QByteArray, QByteArray>> out;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const int c = line.indexOf(QLatin1Char(':'));
        if (c <= 0) continue;
        out.append({line.left(c).trimmed().toUtf8(), line.mid(c + 1).trimmed().toUtf8()});
    }
    return out;
}
}

namespace Uploads {
QStringList providerIds() { return {QStringLiteral("custom"), QStringLiteral("smms"), QStringLiteral("imgur")}; }

std::unique_ptr<UploadProvider> create(const Config& c)
{
    if (c.provider == QLatin1String("smms"))
        return std::make_unique<HttpProvider>(QStringLiteral("smms"), QStringLiteral("SM.MS"), QUrl(QStringLiteral("https://sm.ms/api/v2/upload")),
                                              QStringLiteral("smfile"), QStringLiteral("data.url"),
                                              QList<QPair<QByteArray, QByteArray>>{{"Authorization", c.secret.trimmed().toUtf8()}}, false,
                                              QStringLiteral("SM.MS API Token"));
    if (c.provider == QLatin1String("imgur"))
        return std::make_unique<HttpProvider>(QStringLiteral("imgur"), QStringLiteral("Imgur"), QUrl(QStringLiteral("https://api.imgur.com/3/image")),
                                              QStringLiteral("image"), QStringLiteral("data.link"),
                                              QList<QPair<QByteArray, QByteArray>>{{"Authorization", "Client-ID " + c.secret.trimmed().toUtf8()}},
                                              false, QStringLiteral("Imgur Client ID"));
    return std::make_unique<HttpProvider>(QStringLiteral("custom"), QCoreApplication::translate("QingYing", "自定义 HTTP"), QUrl(c.url), c.field.isEmpty() ? QStringLiteral("file") : c.field,
                                          c.jsonPath, parseHeaders(c.secret), true, QCoreApplication::translate("QingYing", "请求头，每行一个“名称: 值”"));
}

QString jsonValue(const QByteArray& json, const QString& path) { return Logic::jsonValue(json, path); }

void upload(const QImage& image, const Config& cfg, std::function<void(QString, QString)> done)
{
    auto provider = std::shared_ptr<UploadProvider>(create(cfg));
    if (!provider->request().url().isValid() || provider->request().url().scheme().isEmpty()) {
        done({}, QCoreApplication::translate("QingYing", "上传地址无效"));
        return;
    }
    QImage img = image;
    img.setDevicePixelRatio(1);
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    QHttpMultiPart* mp = provider->body(png);
    QNetworkReply* reply = net()->post(provider->request(), mp);
    mp->setParent(reply);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, provider, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, reply->errorString());
            return;
        }
        QString err;
        const QString url = provider->parseUrl(data, &err);
        done(url, err);
    });
}

void uploadWithSettings(const QImage& image)
{
    if (!Settings::uploadEnabled()) {
        Notify::warn(QCoreApplication::translate("QingYing", "上传未启用"), QCoreApplication::translate("QingYing", "请先在设置 → 上传中启用并配置"));
        return;
    }
    Notify::info(QCoreApplication::translate("QingYing", "正在上传…"), QString());
    upload(image, Settings::uploadConfig(), [](const QString& url, const QString& err) {
        if (url.isEmpty()) {
            Notify::warn(QCoreApplication::translate("QingYing", "上传失败"), err);
            return;
        }
        QGuiApplication::clipboard()->setText(url);
        Notify::info(QCoreApplication::translate("QingYing", "上传成功，链接已复制"), url);
    });
}
}
