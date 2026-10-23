#include <QCoreApplication>
#include "Updater.h"
#include "Diagnostics.h"
#include "Logic.h"
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
#include <QNetworkRequest>
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

void Updater::finishManualBusy()
{
    if (m_busyDlg) {
        m_busyDlg->close();
        m_busyDlg->deleteLater();
        m_busyDlg = nullptr;
    }
}

void Updater::reportManual(const QString& title, const QString& text, bool warn)
{
    finishManualBusy();
    QMessageBox box(warn ? QMessageBox::Warning : QMessageBox::Information, title, text);
    box.setWindowIcon(appIcon());
    box.setWindowFlag(Qt::WindowStaysOnTopHint);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}

void Updater::applyRelease(const Release& r, bool manual)
{
    const QString cur = QCoreApplication::applicationVersion();
    Diagnostics::log(QStringLiteral("update compare remote=%1 current=%2").arg(r.tag, cur));
    if (Logic::isNewerVersion(r.tag, cur)) {
        finishManualBusy();
        offer(r);
    } else if (manual) {
        reportManual(QCoreApplication::translate("QingYing", "已是最新版本"),
                     QCoreApplication::translate("QingYing", "当前版本 %1，无需更新。").arg(cur), false);
    } else {
        finishManualBusy();
    }
}

void Updater::tryHtmlFallback(bool manual)
{
    Diagnostics::log(QStringLiteral("update fallback: GET %1").arg(QString::fromLatin1(kLatestPage)));
    QNetworkRequest req = request(QUrl(QString::fromLatin1(kLatestPage)));
    req.setRawHeader("Accept", "text/html");
    QNetworkReply* reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        m_busy = false;
        Settings::setLastUpdateCheck(QDateTime::currentDateTime());
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl finalUrl = reply->url();
        const QString tag = Logic::releaseTagFromUrl(finalUrl);
        Diagnostics::log(QStringLiteral("update fallback http=%1 err=%2 url=%3 tag=%4")
                             .arg(http)
                             .arg(reply->errorString())
                             .arg(finalUrl.toString(), tag));
        if (reply->error() != QNetworkReply::NoError || tag.isEmpty()) {
            if (manual) {
                QString detail = reply->errorString();
                if (reply->error() == QNetworkReply::TimeoutError)
                    detail = QCoreApplication::translate("QingYing", "连接超时，请检查网络后重试");
                else if (reply->error() == QNetworkReply::HostNotFoundError
                         || reply->error() == QNetworkReply::ConnectionRefusedError)
                    detail = QCoreApplication::translate("QingYing", "无法连接 GitHub，请检查网络或稍后重试");
                else if (detail.isEmpty())
                    detail = QCoreApplication::translate("QingYing", "无法解析最新版本，请打开 GitHub 发布页手动下载");
                reportManual(QCoreApplication::translate("QingYing", "检查更新失败"), detail, true);
            } else {
                finishManualBusy();
            }
            return;
        }
        Release r;
        r.tag = tag;
        r.version = QVersionNumber::fromString(tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag);
        r.page = finalUrl;
        // 无 API 资源列表时，安装版也走发布页（避免误下错误包）
        applyRelease(r, manual);
    });
}

void Updater::handleApiResult(const QByteArray& body, int http, int err, const QString& errStr, bool manual)
{
    Release r;
    const bool ok = (err == QNetworkReply::NoError) && parse(body, &r);
    Diagnostics::log(QStringLiteral("update api http=%1 err=%2 parsed=%3 tag=%4")
                         .arg(http)
                         .arg(errStr)
                         .arg(ok)
                         .arg(r.tag));
    if (ok) {
        m_busy = false;
        applyRelease(r, manual);
        return;
    }
    const bool rateLimited = http == 403 || http == 429
        || QString::fromUtf8(body).contains(QLatin1String("rate limit"), Qt::CaseInsensitive);
    // API 失败时尝试网页跳转回退（限流、空 body、JSON 异常等）
    if (rateLimited || err != QNetworkReply::NoError || body.isEmpty()) {
        tryHtmlFallback(manual);
        return;
    }
    m_busy = false;
    if (manual) {
        reportManual(QCoreApplication::translate("QingYing", "检查更新失败"),
                     QCoreApplication::translate("QingYing", "无法解析更新信息，请稍后重试或打开 GitHub 发布页"), true);
    } else {
        finishManualBusy();
    }
}

void Updater::check(bool manual)
{
    if (m_busy) {
        if (manual) {
            // 勿调用 reportManual：会关掉当前进度框
            QMessageBox::information(nullptr,
                                     QCoreApplication::translate("QingYing", "检查更新"),
                                     QCoreApplication::translate("QingYing", "正在检查更新，请稍候…"));
        }
        return;
    }
    m_busy = true;
    Diagnostics::log(QStringLiteral("update check manual=%1 ver=%2").arg(manual).arg(QCoreApplication::applicationVersion()));
    if (manual) {
        finishManualBusy();
        m_busyDlg = new QProgressDialog(QCoreApplication::translate("QingYing", "正在检查更新…"), QString(), 0, 0);
        m_busyDlg->setWindowTitle(QCoreApplication::translate("QingYing", "青影 · 更新"));
        m_busyDlg->setWindowIcon(appIcon());
        m_busyDlg->setWindowModality(Qt::ApplicationModal);
        m_busyDlg->setMinimumDuration(0);
        m_busyDlg->setCancelButton(nullptr);
        m_busyDlg->setWindowFlag(Qt::WindowStaysOnTopHint);
        m_busyDlg->show();
        QApplication::processEvents();
    }
    QNetworkReply* reply = m_net.get(request(QUrl(QString::fromLatin1(kLatestApi))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, manual] {
        reply->deleteLater();
        Settings::setLastUpdateCheck(QDateTime::currentDateTime());
        const QByteArray body = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        handleApiResult(body, http, int(reply->error()), reply->errorString(), manual);
    });
}

void Updater::offer(const Release& r)
{
    QMessageBox box;
    box.setWindowTitle(QCoreApplication::translate("QingYing", "青影 · 发现新版本"));
    box.setWindowIcon(appIcon());
    box.setIconPixmap(appIcon().pixmap(48, 48));
    box.setText(QCoreApplication::translate("QingYing", "<b>青影 %1 已发布</b>（当前 %2）")
                    .arg(r.tag, QCoreApplication::applicationVersion()));
    const bool openPageOnly = isPortable() || r.setupUrl.isEmpty() || r.sumsUrl.isEmpty();
    box.setInformativeText(openPageOnly
                               ? QCoreApplication::translate("QingYing", "将打开发布页下载新版本。")
                               : QCoreApplication::translate("QingYing", "将下载安装包并校验 SHA-256，校验通过后启动安装程序并退出青影。"));
    box.setDetailedText(r.notes);
    QPushButton* go = box.addButton(openPageOnly ? QCoreApplication::translate("QingYing", "打开发布页")
                                                 : QCoreApplication::translate("QingYing", "立即更新"),
                                    QMessageBox::AcceptRole);
    box.addButton(QCoreApplication::translate("QingYing", "稍后"), QMessageBox::RejectRole);
    box.setWindowFlag(Qt::WindowStaysOnTopHint);
    box.exec();
    if (box.clickedButton() != go) return;
    if (openPageOnly) QDesktopServices::openUrl(r.page.isValid() ? r.page : QUrl(QString::fromLatin1(kLatestPage)));
    else download(r);
}

void Updater::download(const Release& r)
{
    auto* progress = new QProgressDialog(QCoreApplication::translate("QingYing", "正在下载 %1 …").arg(r.setupName),
                                         QCoreApplication::translate("QingYing", "取消"), 0, 100);
    progress->setWindowTitle(QCoreApplication::translate("QingYing", "青影 · 更新"));
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
                QMessageBox::warning(nullptr, QCoreApplication::translate("QingYing", "下载更新失败"), setup->errorString());
            return;
        }
        const QByteArray data = setup->readAll();
        const QString actual = Logic::sha256Hex(data);
        const QString expected = Logic::expectedSha256(QString::fromUtf8(sums->readAll()), r.setupName);
        if (expected.isEmpty() || expected != actual) {
            QMessageBox::warning(nullptr, QCoreApplication::translate("QingYing", "青影 · 更新"),
                                 QCoreApplication::translate("QingYing", "安装包 SHA-256 校验失败，已放弃安装。\n期望：%1\n实际：%2")
                                     .arg(expected.isEmpty() ? QCoreApplication::translate("QingYing", "（未找到）") : expected, actual));
            return;
        }
        const QString path = QDir::temp().filePath(r.setupName);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
            QMessageBox::warning(nullptr, QCoreApplication::translate("QingYing", "下载更新失败"),
                                 QCoreApplication::translate("QingYing", "无法写入 %1").arg(path));
            return;
        }
        f.close();
        if (QMessageBox::question(nullptr, QCoreApplication::translate("QingYing", "青影 · 更新"),
                                  QCoreApplication::translate("QingYing", "安装包已下载并通过校验。现在启动安装程序并退出青影吗？"))
            != QMessageBox::Yes)
            return;
        if (QProcess::startDetached(path, {})) {
            Diagnostics::log(QStringLiteral("installer launched, quitting: %1").arg(path));
            qApp->quit();
        } else {
            QMessageBox::warning(nullptr, QCoreApplication::translate("QingYing", "无法启动安装程序"), path);
        }
    };
    connect(sums, &QNetworkReply::finished, this, finish);
    connect(setup, &QNetworkReply::finished, this, finish);
}
