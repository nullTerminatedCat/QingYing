#include <QCoreApplication>
#include "Theme.h"
#include "ResultDialog.h"
#include "Icons.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

ResultDialog::ResultDialog(const QString& title, QWidget* parent) : QDialog(parent)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QCoreApplication::translate("QingYing", "青影 · %1").arg(title));
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(520, 380);
    Theme::setSheet(this, QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel#title { font-size: 20px; font-weight: 600; color: #1C1C1E; }
        QLabel#status { font-size: 12px; color: #6E6E73; }
        QLabel#error { font-size: 12px; color: #FF453A; }
        QPlainTextEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; padding: 8px;
            font-size: 14px; color: #1C1C1E; selection-background-color: #4F6BFF; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 10px; padding: 6px 16px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
        QPushButton:disabled { color: #A1A1A6; }
    )"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);
    auto* t = new QLabel(title, this);
    t->setObjectName(QStringLiteral("title"));
    root->addWidget(t);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    m_edit = new QPlainTextEdit(this);
    root->addWidget(m_edit, 1);
    auto* row = new QHBoxLayout;
    m_open = new QPushButton(QCoreApplication::translate("QingYing", "打开链接"), this);
    m_open->hide();
    row->addWidget(m_open);
    row->addStretch();
    auto* close = new QPushButton(QCoreApplication::translate("QingYing", "关闭"), this);
    m_copy = new QPushButton(QCoreApplication::translate("QingYing", "复制"), this);
    m_copy->setObjectName(QStringLiteral("primary"));
    m_copy->setEnabled(false);
    row->addWidget(close);
    row->addWidget(m_copy);
    root->addLayout(row);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QString sel = m_edit->textCursor().selectedText();
        QGuiApplication::clipboard()->setText(sel.isEmpty() ? m_edit->toPlainText() : sel.replace(QChar(0x2029), QLatin1Char('\n')));
        m_copy->setText(QCoreApplication::translate("QingYing", "已复制"));
    });
    connect(m_edit, &QPlainTextEdit::textChanged, this, [this] { m_copy->setText(QCoreApplication::translate("QingYing", "复制")); });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QCoreApplication::translate("QingYing", "打开链接"),
                                  QCoreApplication::translate("QingYing", "确定在浏览器中打开以下链接？\n\n%1").arg(m_url)) == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl(m_url));
    });
}

void ResultDialog::setStatus(const QString& s, bool error)
{
    m_status->setObjectName(error ? QStringLiteral("error") : QStringLiteral("status"));
    Theme::setSheet(m_status, QString());
    m_status->setText(s);
}

void ResultDialog::setText(const QString& text)
{
    m_edit->setPlainText(text);
    m_copy->setEnabled(!text.isEmpty());
}

void ResultDialog::setUrl(const QString& url)
{
    m_url = url;
    m_open->setVisible(!url.isEmpty());
    m_open->setToolTip(url);
}
