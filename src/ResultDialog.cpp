#include <QCoreApplication>
#include "Theme.h"
#include "ResultDialog.h"
#include "Icons.h"
#include "Settings.h"
#include "Translator.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

ResultDialog::ResultDialog(const QString& title, QWidget* parent) : QDialog(parent), m_panelTitle(title)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(560, 400);
    Theme::setSheet(this, QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel#title { font-size: 20px; font-weight: 600; color: #1C1C1E; }
        QLabel#status { font-size: 12px; color: #6E6E73; }
        QLabel#error { font-size: 12px; color: #FF453A; }
        QPlainTextEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; padding: 8px;
            font-size: 14px; color: #1C1C1E; selection-background-color: #4F6BFF; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 10px; padding: 6px 14px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
        QPushButton:disabled { color: #A1A1A6; }
    )"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("title"));
    root->addWidget(m_titleLabel);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    root->addWidget(m_status);
    m_edit = new QPlainTextEdit(this);
    root->addWidget(m_edit, 1);

    auto* tools = new QHBoxLayout;
    tools->setSpacing(8);
    m_open = new QPushButton(this);
    m_open->hide();
    m_auto = new QPushButton(this);
    m_toZh = new QPushButton(this);
    m_toEn = new QPushButton(this);
    m_restore = new QPushButton(this);
    m_restore->hide();
    for (QPushButton* b : {m_open, m_auto, m_toZh, m_toEn, m_restore}) tools->addWidget(b);
    tools->addStretch();
    root->addLayout(tools);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    row->addStretch();
    m_close = new QPushButton(this);
    m_copy = new QPushButton(this);
    m_copy->setObjectName(QStringLiteral("primary"));
    m_copy->setEnabled(false);
    row->addWidget(m_close);
    row->addWidget(m_copy);
    root->addLayout(row);

    m_copyFlash = new QTimer(this);
    m_copyFlash->setSingleShot(true);
    connect(m_copyFlash, &QTimer::timeout, this, [this] {
        if (m_copy) m_copy->setText(QCoreApplication::translate("QingYing", "复制"));
    });

    connect(m_close, &QPushButton::clicked, this, &QDialog::close);
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QString sel = m_edit->textCursor().selectedText();
        QGuiApplication::clipboard()->setText(sel.isEmpty() ? m_edit->toPlainText()
                                                            : sel.replace(QChar(0x2029), QLatin1Char('\n')));
        flashCopyDone();
    });
    connect(m_edit, &QPlainTextEdit::textChanged, this, [this] {
        if (!m_copyFlash->isActive()) m_copy->setText(QCoreApplication::translate("QingYing", "复制"));
        m_copy->setEnabled(!m_busy && !m_edit->toPlainText().isEmpty());
        if (!m_busy) {
            const bool has = !m_edit->toPlainText().trimmed().isEmpty();
            m_auto->setEnabled(has);
            m_toZh->setEnabled(has);
            m_toEn->setEnabled(has);
        }
    });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QCoreApplication::translate("QingYing", "打开链接"),
                                  QCoreApplication::translate("QingYing", "确定在浏览器中打开以下链接？\n\n%1").arg(m_url))
            == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl(m_url));
    });
    connect(m_auto, &QPushButton::clicked, this, [this] {
        const bool toZh = Translator::suggestTarget(m_edit->toPlainText()) == Translator::Target::Chinese;
        startTranslate(toZh);
    });
    connect(m_toZh, &QPushButton::clicked, this, [this] { startTranslate(true); });
    connect(m_toEn, &QPushButton::clicked, this, [this] { startTranslate(false); });
    connect(m_restore, &QPushButton::clicked, this, [this] {
        if (m_original.isEmpty()) return;
        m_edit->setPlainText(m_original);
        m_restore->hide();
        setStatus(QCoreApplication::translate("QingYing", "已还原为识别原文，可继续编辑或重新翻译"));
    });

    retranslateUi();
    setTranslateEnabled(false);
}

void ResultDialog::retranslateUi()
{
    setWindowTitle(m_panelTitle);
    if (m_titleLabel) m_titleLabel->setText(m_panelTitle);
    m_open->setText(QCoreApplication::translate("QingYing", "打开链接"));
    m_auto->setText(QCoreApplication::translate("QingYing", "翻译"));
    m_toZh->setText(QCoreApplication::translate("QingYing", "译成中文"));
    m_toEn->setText(QCoreApplication::translate("QingYing", "译成英文"));
    m_restore->setText(QCoreApplication::translate("QingYing", "还原原文"));
    m_close->setText(QCoreApplication::translate("QingYing", "关闭"));
    if (!m_copyFlash->isActive()) m_copy->setText(QCoreApplication::translate("QingYing", "复制"));
}

void ResultDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) retranslateUi();
}

void ResultDialog::flashCopyDone()
{
    m_copy->setText(QCoreApplication::translate("QingYing", "已复制"));
    m_copyFlash->start(1500);
}

void ResultDialog::setTranslateEnabled(bool on)
{
    m_wantTranslate = on;
    const bool show = on && Settings::translateEnabled();
    m_auto->setVisible(show);
    m_toZh->setVisible(show);
    m_toEn->setVisible(show);
    if (!show) m_restore->hide();
}

void ResultDialog::setWorking(bool working) { setBusy(working); }

void ResultDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_edit->setReadOnly(busy);
    const bool has = !m_edit->toPlainText().trimmed().isEmpty();
    m_auto->setEnabled(!busy && has);
    m_toZh->setEnabled(!busy && has);
    m_toEn->setEnabled(!busy && has);
    m_restore->setEnabled(!busy && !m_original.isEmpty());
    m_copy->setEnabled(!busy && !m_edit->toPlainText().isEmpty());
    m_open->setEnabled(!busy && !m_url.isEmpty());
}

void ResultDialog::startTranslate(bool toChinese)
{
    const QString src = m_edit->toPlainText();
    if (src.trimmed().isEmpty()) {
        setStatus(QCoreApplication::translate("QingYing", "没有可翻译的文字"), true);
        return;
    }
    if (!Settings::translateEnabled()) {
        setTranslateEnabled(m_wantTranslate);
        setStatus(QCoreApplication::translate("QingYing", "已在设置中关闭联网翻译"), true);
        return;
    }
    if (m_original.isEmpty()) m_original = src;
    const int gen = ++m_translateGen;
    setBusy(true);
    setStatus(Settings::aiTranslateReady() ? QCoreApplication::translate("QingYing", "正在用 AI 翻译…")
                                           : QCoreApplication::translate("QingYing", "正在翻译…"));
    QPointer<ResultDialog> guard(this);
    const auto target = toChinese ? Translator::Target::Chinese : Translator::Target::English;
    Translator::translate(src, target, [guard, toChinese, gen](Translator::Result r) {
        if (!guard || gen != guard->m_translateGen) return;
        guard->setBusy(false);
        if (!r.ok) {
            guard->setStatus(QCoreApplication::translate("QingYing", "翻译失败：%1").arg(r.error), true);
            return;
        }
        const QString lang = toChinese ? QCoreApplication::translate("QingYing", "中文")
                                       : QCoreApplication::translate("QingYing", "英文");
        if (r.local == TranslateText::ReuseKind::Already) {
            guard->setStatus(QCoreApplication::translate("QingYing", "已是%1，未请求翻译").arg(lang));
            return;
        }
        guard->m_edit->setPlainText(r.text);
        const bool backToOriginal = !guard->m_original.isEmpty() && r.text == guard->m_original;
        if (backToOriginal)
            guard->m_restore->hide();
        else {
            guard->m_restore->show();
            guard->m_restore->setEnabled(true);
        }
        if (r.local == TranslateText::ReuseKind::RoundTrip) {
            guard->setStatus(QCoreApplication::translate("QingYing", "已改回翻译前的文字，未再请求翻译"));
            return;
        }
        if (r.local == TranslateText::ReuseKind::Repeat) {
            guard->setStatus(QCoreApplication::translate("QingYing", "与刚才的译文相同，未再请求翻译"));
            return;
        }
        if (r.fellBack)
            guard->setStatus(QCoreApplication::translate("QingYing", "AI 不可用，已改用 MyMemory · 已译成%1").arg(lang));
        else if (r.engine == QLatin1String("ai"))
            guard->setStatus(QCoreApplication::translate("QingYing", "已用 AI 译成%1 · 可编辑后复制，或点“还原原文”").arg(lang));
        else
            guard->setStatus(QCoreApplication::translate("QingYing", "已译成%1 · 可编辑后复制，或点“还原原文”").arg(lang));
    });
}

void ResultDialog::setStatus(const QString& s, bool error)
{
    m_status->setObjectName(error ? QStringLiteral("error") : QStringLiteral("status"));
    if (m_status->style()) {
        m_status->style()->unpolish(m_status);
        m_status->style()->polish(m_status);
    }
    m_status->update();
    m_status->setText(s);
}

void ResultDialog::setText(const QString& text)
{
    Translator::forgetRecall();
    m_original.clear();
    m_restore->hide();
    m_edit->setPlainText(text);
    m_copy->setEnabled(!m_busy && !text.isEmpty());
}

void ResultDialog::setUrl(const QString& url)
{
    m_url = url;
    m_open->setVisible(!url.isEmpty());
    m_open->setEnabled(!m_busy && !url.isEmpty());
    m_open->setToolTip(url);
}