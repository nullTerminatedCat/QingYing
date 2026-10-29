#include "OcrModels.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {
const QString kDetName = QStringLiteral("ch_PP-OCRv4_det_mobile.onnx");
const QString kRecName = QStringLiteral("ch_PP-OCRv4_rec_mobile.onnx");
const QString kClsName = QStringLiteral("ch_ppocr_mobile_v2.0_cls_mobile.onnx");
const QString kDictName = QStringLiteral("ppocr_keys_v1.txt");
const QString kDetSha = QStringLiteral("d2a7720d45a54257208b1e13e36a8479894cb74155a5efe29462512d42f49da9");
const QString kRecSha = QStringLiteral("48fc40f24f6d2a207a2b1091d3437eb3cc3eb6b676dc3ef9c37384005483683b");
const QString kClsSha = QStringLiteral("e47acedf663230f8863ff1ab0e64dd2d82b838fceb5957146dab185a89d6215c");
const QString kDictSha = QStringLiteral("28b2362ad4ab2dc38769aa72feb535e3a9ddb3fd2a7585a05920e6393b1dc7f7");
const QString kBase = QStringLiteral("https://www.modelscope.cn/models/RapidAI/RapidOCR/resolve/v3.10.0");

QMutex& mutex()
{
    static QMutex m;
    return m;
}

QString hashFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

bool hashOk(const QString& path, const QString& expect)
{
    return QFileInfo(path).size() > 0 && hashFile(path).compare(expect, Qt::CaseInsensitive) == 0;
}

QString pick(const QDir& dir, const QString& official, const QString& sha, const QStringList& legacy, qint64 minSize)
{
    const QString primary = dir.filePath(official);
    if (QFileInfo::exists(primary)) return hashOk(primary, sha) ? primary : QString();
    for (const QString& name : legacy) {
        const QString path = dir.filePath(name);
        if (QFileInfo(path).size() >= minSize) return path;
    }
    return {};
}

QStringList searchDirs()
{
    QStringList dirs;
    const QString env = qEnvironmentVariable("QINGYING_OCR_MODELS");
    if (!env.isEmpty()) dirs << env;
    if (QCoreApplication::instance())
        dirs << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("models"));
    const QString local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!local.isEmpty()) dirs << QDir(local).filePath(QStringLiteral("models"));
    dirs.removeDuplicates();
    return dirs;
}

OcrModels::Paths fromDir(const QString& directory)
{
    OcrModels::Paths p;
    p.directory = directory;
    const QDir dir(directory);
    p.det = pick(dir, kDetName, kDetSha, {QStringLiteral("ch_PP-OCRv4_det_infer.onnx")}, 100000);
    p.rec = pick(dir, kRecName, kRecSha, {QStringLiteral("ch_PP-OCRv4_rec_infer.onnx")}, 100000);
    p.cls = pick(dir, kClsName, kClsSha, {QStringLiteral("ch_ppocr_mobile_v2.0_cls_infer.onnx")}, 10000);
    p.dict = pick(dir, kDictName, kDictSha, {}, 1000);
    p.ready = !p.det.isEmpty() && !p.rec.isEmpty() && !p.dict.isEmpty();
    return p;
}

struct Spec {
    QString name;
    QString sha;
    QString urlPath;
    QString label;
};

QByteArray httpGet(const QUrl& url, QString* error)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QingYing/%1").arg(QCoreApplication::applicationVersion()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(120000);
    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    const auto fail = [&](const QString& msg) {
        if (error) *error = msg;
        reply->deleteLater();
        return QByteArray();
    };
    if (reply->error() != QNetworkReply::NoError) return fail(reply->errorString());
    const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    if (http >= 400 || body.size() < 32) {
        if (error) *error = QCoreApplication::translate("QingYing", "下载失败（HTTP %1）").arg(http);
        return {};
    }
    return body;
}

bool writeVerified(const QString& path, const QByteArray& data, const QString& sha, QString* error)
{
    const QString got = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    if (got.compare(sha, Qt::CaseInsensitive) != 0) {
        if (error) *error = QCoreApplication::translate("QingYing", "模型校验失败：%1").arg(QFileInfo(path).fileName());
        return false;
    }
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        if (error) *error = QCoreApplication::translate("QingYing", "无法写入 %1").arg(QDir::toNativeSeparators(path));
        return false;
    }
    return true;
}
} // namespace

namespace OcrModels {
Paths locate()
{
    for (const QString& dir : searchDirs()) {
        const Paths p = fromDir(dir);
        if (p.ready) return p;
    }
    Paths empty;
    empty.directory = preferredDirectory();
    return empty;
}

QString preferredDirectory()
{
    const QString env = qEnvironmentVariable("QINGYING_OCR_MODELS");
    if (!env.isEmpty()) {
        QDir().mkpath(env);
        return env;
    }
    if (QCoreApplication::instance()) {
        const QString beside = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("models"));
        if (QDir().mkpath(beside)) {
            QFile probe(QDir(beside).filePath(QStringLiteral(".qywrite")));
            if (probe.open(QIODevice::WriteOnly)) {
                probe.close();
                probe.remove();
                return beside;
            }
        }
    }
    const QString local = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral("models"));
    QDir().mkpath(local);
    return local;
}

bool ensure(QString* error, const std::function<void(const QString&)>& progress)
{
    QMutexLocker lock(&mutex());
    if (locate().ready) return true;
    const QString dir = preferredDirectory();
    if (dir.isEmpty() || !QDir().mkpath(dir)) {
        if (error) *error = QCoreApplication::translate("QingYing", "无法创建模型目录");
        return false;
    }
    const Spec files[] = {
        {kDetName, kDetSha, QStringLiteral("/onnx/PP-OCRv4/det/ch_PP-OCRv4_det_mobile.onnx"),
         QCoreApplication::translate("QingYing", "正在下载检测模型…")},
        {kRecName, kRecSha, QStringLiteral("/onnx/PP-OCRv4/rec/ch_PP-OCRv4_rec_mobile.onnx"),
         QCoreApplication::translate("QingYing", "正在下载识别模型…")},
        {kClsName, kClsSha, QStringLiteral("/onnx/PP-OCRv4/cls/ch_ppocr_mobile_v2.0_cls_mobile.onnx"),
         QCoreApplication::translate("QingYing", "正在下载方向模型…")},
        {kDictName, kDictSha, QStringLiteral("/paddle/PP-OCRv4/rec/ch_PP-OCRv4_rec_mobile/ppocr_keys_v1.txt"),
         QCoreApplication::translate("QingYing", "正在下载字典…")},
    };
    for (const Spec& spec : files) {
        const QString path = QDir(dir).filePath(spec.name);
        if (hashOk(path, spec.sha)) continue;
        if (progress) progress(spec.label);
        QString getError;
        const QByteArray data = httpGet(QUrl(kBase + spec.urlPath), &getError);
        if (data.isEmpty() || !writeVerified(path, data, spec.sha, &getError)) {
            if (error) {
                *error = getError.isEmpty() ? QCoreApplication::translate("QingYing", "下载 %1 失败").arg(spec.name) : getError;
            }
            return false;
        }
    }
    if (!fromDir(dir).ready) {
        if (error) *error = QCoreApplication::translate("QingYing", "模型文件不完整");
        return false;
    }
    return true;
}
}
