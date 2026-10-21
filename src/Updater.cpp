#include "Updater.h"
#include "Icons.h"
#include "Notify.h"
#include "Settings.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <memory>

Updater::Updater(QObject* parent) : QObject(parent) {}

bool Updater::isPortable()
{
    return !QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("unins000.exe")));
}

QNetworkRequest Updater::request(const QUrl& url) const
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("QingYing/%1").arg(QCoreApplication::applicationVersion()));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(30000);
    return req;
}

bool Updater::parse(const QByteArray& json, Release* out)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    out->tag = o.value(QStringLiteral("tag_name")).toString();
    if (out->tag.isEmpty()) return false;
    out->version = QVersionNumber::fromString(out->tag.startsWith(QLatin1Char('v')) ? out->tag.mid(1) : out->tag);
    out->notes = o.value(QStringLiteral("body")).toString();
    out->page = QUrl(o.value(QStringLiteral("html_url")).toString());
    for (const QJsonValue& v : o.value(QStringLiteral("assets")).toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        const QUrl url(a.value(QStringLiteral("browser_download_url")).toString());
        if (name.endsWith(QLatin1String("-x64-setup.exe"))) {
            out->setupUrl = url;
            out->setupName = name;
        } else if (name.startsWith(QLatin1String("SHA256SUMS")) && name.contains(QLatin1String("x64"))) {
            out->sumsUrl = url;
        }
    }
    return true;
}

void Updater::maybeAutoCheck()
{
    if (!Settings::autoUpdate()) return;
    const QDateTime last = Settings::lastUpdateCheck();
    if (last.isValid() && last.secsTo(QDateTime::currentDateTime()) < 24 * 3600) return;
    check(false);
}

void Updater::check(bool manual)
{
    if (m_busy) return;
    m_busy = true;
    QNetworkReply* reply = m_net.get(request(QUrl(QString::fromLatin1(kLatestApi))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        m_busy = false;
        Settings::setLastUpdateCheck(QDateTime::currentDateTime());
        Release r;
        if (reply->error() != QNetworkReply::NoError || !parse(reply->readAll(), &r)) {
            if (manual) Notify::warn(QStringLiteral("检查更新失败"), reply->errorString());
            return;
        }
        const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
        if (r.version > current) offer(r);
        else if (manual) Notify::info(QStringLiteral("已是最新版本"), QStringLiteral("当前版本 %1").arg(current.toString()));
    });
}

void Updater::offer(const Release& r)
{
    QMessageBox box;
    box.setWindowTitle(QStringLiteral("青影 · 发现新版本"));
    box.setWindowIcon(appIcon());
    box.setIconPixmap(appIcon().pixmap(48, 48));
    box.setText(QStringLiteral("<b>青影 %1 已发布</b>（当前 %2）").arg(r.tag, QCoreApplication::applicationVersion()));
    box.setInformativeText(isPortable() ? QStringLiteral("你正在使用免安装版，将打开发布页下载新版本。")
                                        : QStringLiteral("将下载安装包并校验 SHA-256，校验通过后启动安装程序并退出青影。"));
    box.setDetailedText(r.notes);
    QPushButton* go = box.addButton(isPortable() ? QStringLiteral("打开发布页") : QStringLiteral("立即更新"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("稍后"), QMessageBox::RejectRole);
    box.setWindowFlag(Qt::WindowStaysOnTopHint);
    box.exec();
    if (box.clickedButton() != go) return;
    if (isPortable() || r.setupUrl.isEmpty() || r.sumsUrl.isEmpty()) QDesktopServices::openUrl(r.page);
    else download(r);
}

void Updater::download(const Release& r)
{
    auto* progress = new QProgressDialog(QStringLiteral("正在下载 %1 …").arg(r.setupName), QStringLiteral("取消"), 0, 100);
    progress->setWindowTitle(QStringLiteral("青影 · 更新"));
    progress->setWindowIcon(appIcon());
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->setMinimumDuration(0);
    progress->show();
    QNetworkReply* sums = m_net.get(request(r.sumsUrl));
    QNetworkReply* setup = m_net.get(request(r.setupUrl));
    connect(progress, &QProgressDialog::canceled, setup, &QNetworkReply::abort);
    connect(setup, &QNetworkReply::downloadProgress, progress, [progress](qint64 got, qint64 total) {
        if (total > 0) progress->setValue(int(got * 100 / total));
    });
    auto done = std::make_shared<int>(0);
    auto finish = [this, sums, setup, progress, r, done] {
        if (++*done < 2) return;
        progress->close();
        sums->deleteLater();
        setup->deleteLater();
        if (setup->error() != QNetworkReply::NoError) {
            if (setup->error() != QNetworkReply::OperationCanceledError)
                Notify::warn(QStringLiteral("下载更新失败"), setup->errorString());
            return;
        }
        const QByteArray data = setup->readAll();
        const QString actual = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
        QString expected;
        for (const QString& line : QString::fromUtf8(sums->readAll()).split(QLatin1Char('\n'))) {
            const QStringList parts = line.simplified().split(QLatin1Char(' '));
            if (parts.size() >= 2 && parts.last() == r.setupName) expected = parts.first().toLower();
        }
        if (expected.isEmpty() || expected != actual) {
            QMessageBox::warning(nullptr, QStringLiteral("青影 · 更新"),
                                 QStringLiteral("安装包 SHA-256 校验失败，已放弃安装。\n期望：%1\n实际：%2")
                                     .arg(expected.isEmpty() ? QStringLiteral("（未找到）") : expected, actual));
            return;
        }
        const QString path = QDir::temp().filePath(r.setupName);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
            Notify::warn(QStringLiteral("下载更新失败"), QStringLiteral("无法写入 %1").arg(path));
            return;
        }
        f.close();
        if (QMessageBox::question(nullptr, QStringLiteral("青影 · 更新"),
                                  QStringLiteral("安装包已下载并通过校验。现在启动安装程序并退出青影吗？")) != QMessageBox::Yes)
            return;
        if (QProcess::startDetached(path, {})) qApp->quit();
        else Notify::warn(QStringLiteral("无法启动安装程序"), path);
    };
    connect(sums, &QNetworkReply::finished, this, finish);
    connect(setup, &QNetworkReply::finished, this, finish);
}
