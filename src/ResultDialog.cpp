#include <QCoreApplication>
#include "Theme.h"
#include "ResultDialog.h"
#include "Icons.h"
#include "Notify.h"
#include "Settings.h"
#include "SettingsRequest.h"
#include "Translator.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString aiNeedText()
{
    return QCoreApplication::translate("QingYing", "请先在设置 → 截图中打开「使用 AI 翻译」，并填写接口地址、密钥和模型");
}

void disarm(QPushButton* button)
{
    if (!button) return;
    button->setAutoDefault(false);
    button->setDefault(false);
}
}

ResultDialog::~ResultDialog()
{
    ++m_translateGen;
    Translator::cancelInFlight();
}

ResultDialog::ResultDialog(const QString& title, QWidget* parent) : QDialog(parent), m_panelTitle(title)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(640, 520);
    Theme::setSheet(this, QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel#title { font-size: 20px; font-weight: 600; color: #1C1C1E; }
        QLabel#status { font-size: 12px; color: #6E6E73; }
        QLabel#error { font-size: 12px; color: #FF453A; }
        QLabel#hint { font-size: 12px; color: #6E6E73; background: transparent; }
        QPlainTextEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; padding: 8px;
            font-size: 14px; color: #1C1C1E; selection-background-color: #4F6BFF; }
        QLineEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; padding: 6px 10px;
            font-size: 13px; color: #1C1C1E; }
        QLineEdit:focus { border: 1px solid #4F6BFF; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 10px; padding: 6px 14px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
        QPushButton#accent { color: #4F6BFF; border: 1px solid #4F6BFF; font-weight: 600; background: #FFFFFF; }
        QPushButton#accent:hover { background: rgba(79,107,255,24); }
        QPushButton#accent:disabled { color: #A1A1A6; border-color: rgba(0,0,0,15); background: #FFFFFF; }
        QPushButton#chip { padding: 5px 12px; border-radius: 8px; }
        QPushButton#chip:hover { border-color: #4F6BFF; color: #4F6BFF; }
        QPushButton#chip:disabled, QPushButton#chip:disabled:hover { color: #A1A1A6; border-color: rgba(0,0,0,15); background: #FFFFFF; }
        QPushButton:disabled { color: #A1A1A6; border-color: rgba(0,0,0,15); }
    )"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName(QStringLiteral("title"));
    root->addWidget(m_titleLabel);

    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(8);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("status"));
    m_status->setWordWrap(true);
    m_openAi = new QPushButton(this);
    m_openAi->setObjectName(QStringLiteral("accent"));
    m_openAi->setCursor(Qt::PointingHandCursor);
    disarm(m_openAi);
    m_openAi->hide();
    statusRow->addWidget(m_status, 1);
    statusRow->addWidget(m_openAi, 0, Qt::AlignTop);
    root->addLayout(statusRow);

    m_edit = new QPlainTextEdit(this);
    root->addWidget(m_edit, 1);

    m_askRow = new QWidget(this);
    auto* askLay = new QHBoxLayout(m_askRow);
    askLay->setContentsMargins(0, 0, 0, 0);
    askLay->setSpacing(8);
    m_question = new QLineEdit(m_askRow);
    m_ask = new QPushButton(m_askRow);
    m_ask->setObjectName(QStringLiteral("accent"));
    m_ask->setMinimumWidth(72);
    m_ask->setCursor(Qt::PointingHandCursor);
    disarm(m_ask);
    askLay->addWidget(m_question, 1);
    askLay->addWidget(m_ask);
    root->addWidget(m_askRow);

    m_rewriteRow = new QWidget(this);
    auto* rewriteLay = new QHBoxLayout(m_rewriteRow);
    rewriteLay->setContentsMargins(0, 0, 0, 0);
    rewriteLay->setSpacing(8);
    auto* rewriteLabel = new QLabel(m_rewriteRow);
    rewriteLabel->setObjectName(QStringLiteral("hint"));
    rewriteLay->addWidget(rewriteLabel);
    const QList<AskText::Mode> modes = AskText::rewriteModes();
    for (AskText::Mode mode : modes) {
        auto* chip = new QPushButton(m_rewriteRow);
        chip->setObjectName(QStringLiteral("chip"));
        chip->setCursor(Qt::PointingHandCursor);
        disarm(chip);
        m_rewriteChips.append(chip);
        rewriteLay->addWidget(chip);
        connect(chip, &QPushButton::clicked, this, [this, mode] { startAssist(mode); });
    }
    rewriteLay->addStretch();
    root->addWidget(m_rewriteRow);

    auto* tools = new QHBoxLayout;
    tools->setSpacing(8);
    m_open = new QPushButton(this);
    m_open->hide();
    m_auto = new QPushButton(this);
    m_toZh = new QPushButton(this);
    m_toEn = new QPushButton(this);
    m_restore = new QPushButton(this);
    m_restore->hide();
    for (QPushButton* b : {m_open, m_auto, m_toZh, m_toEn, m_restore}) {
        disarm(b);
        tools->addWidget(b);
    }
    tools->addStretch();
    root->addLayout(tools);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    row->addStretch();
    m_close = new QPushButton(this);
    m_copy = new QPushButton(this);
    m_copy->setObjectName(QStringLiteral("primary"));
    m_copy->setEnabled(false);
    disarm(m_close);
    disarm(m_copy);
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
        if (!m_busy) syncAssistButtons();
    });
    connect(m_question, &QLineEdit::returnPressed, this, [this] { startAssist(AskText::Mode::Ask); });
    connect(m_ask, &QPushButton::clicked, this, [this] { startAssist(AskText::Mode::Ask); });
    connect(m_openAi, &QPushButton::clicked, this, [] { SettingsRequest::openAi(); });
    connect(m_open, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QCoreApplication::translate("QingYing", "打开链接"),
                                  QCoreApplication::translate("QingYing", "确定在浏览器中打开以下链接？\n\n%1").arg(m_url))
            == QMessageBox::Yes)
            QDesktopServices::openUrl(QUrl(m_url));
    });
    connect(m_auto, &QPushButton::clicked, this, [this] {
        if (m_busy) return;
        // 编辑区已是译文时，自动方向会译回去。改为提示选手动方向，显式按钮仍可用（命中缓存则不联网）。
        if (Translator::textIsCachedTranslation(m_edit->toPlainText())) {
            setStatus(QCoreApplication::translate("QingYing", "当前已是译文，请用「译成中文」或「译成英文」选择方向"));
            return;
        }
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
    setAssistEnabled(false);
    const auto watch = [this](QWidget* w) {
        if (w) w->installEventFilter(this);
    };
    watch(m_edit);
    watch(m_edit->viewport());
    watch(m_question);
    watch(m_ask);
    watch(m_openAi);
    watch(m_open);
    watch(m_auto);
    watch(m_toZh);
    watch(m_toEn);
    watch(m_restore);
    watch(m_close);
    watch(m_copy);
    for (QPushButton* chip : m_rewriteChips) watch(chip);
}

void ResultDialog::retranslateUi()
{
    setWindowTitle(m_panelTitle);
    if (m_titleLabel) m_titleLabel->setText(m_panelTitle);
    m_open->setText(QCoreApplication::translate("QingYing", "打开链接"));
    const QString askKeys = QCoreApplication::translate("QingYing", "Enter 发送。正在编辑正文时，用 Ctrl+Enter 提问。");
    m_question->setPlaceholderText(QCoreApplication::translate("QingYing", "例如：这是什么、怎么修、请解释这段话"));
    m_question->setToolTip(askKeys);
    m_ask->setText(QCoreApplication::translate("QingYing", "提问"));
    m_ask->setToolTip(askKeys);
    if (m_rewriteRow) {
        const auto labels = m_rewriteRow->findChildren<QLabel*>();
        if (!labels.isEmpty()) labels.first()->setText(QCoreApplication::translate("QingYing", "改文案"));
    }
    const QList<AskText::Mode> modes = AskText::rewriteModes();
    for (int i = 0; i < m_rewriteChips.size() && i < modes.size(); ++i) {
        const QString title = AskText::modeTitle(modes.at(i));
        m_rewriteChips.at(i)->setText(title);
        m_rewriteChips.at(i)->setToolTip(title + QStringLiteral("  ") + AskText::shortcutHint(modes.at(i)));
    }
    m_auto->setText(QCoreApplication::translate("QingYing", "翻译"));
    m_toZh->setText(QCoreApplication::translate("QingYing", "译成中文"));
    m_toEn->setText(QCoreApplication::translate("QingYing", "译成英文"));
    m_restore->setText(QCoreApplication::translate("QingYing", "还原原文"));
    m_close->setText(QCoreApplication::translate("QingYing", "关闭"));
    m_close->setToolTip(QCoreApplication::translate("QingYing", "Esc 关闭"));
    m_openAi->setText(QCoreApplication::translate("QingYing", "打开设置"));
    m_openAi->setToolTip(aiNeedText());
    if (!m_copyFlash->isActive()) m_copy->setText(QCoreApplication::translate("QingYing", "复制"));
}

void ResultDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) retranslateUi();
    if (event->type() == QEvent::ActivationChange) noteSettingsReturned();
}

void ResultDialog::keyPressEvent(QKeyEvent* event)
{
    if (handleAssistKey(event)) return;
    QDialog::keyPressEvent(event);
}

bool ResultDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != this && event->type() == QEvent::KeyPress) {
        if (handleAssistKey(static_cast<QKeyEvent*>(event))) return true;
    }
    return QDialog::eventFilter(watched, event);
}

bool ResultDialog::handleAssistKey(QKeyEvent* key)
{
    if (!key) return false;
    if (key->key() == Qt::Key_Escape && (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        if (!m_busy) close();
        return true;
    }
    const Qt::KeyboardModifiers mods = key->modifiers() & ~Qt::KeypadModifier;
    if (!m_wantAssist || mods != Qt::ControlModifier) return false;
    if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
        startAssist(AskText::Mode::Ask);
        return true;
    }
    if (key->key() >= Qt::Key_1 && key->key() <= Qt::Key_4) {
        const QList<AskText::Mode> modes = AskText::rewriteModes();
        const int index = int(key->key() - Qt::Key_1);
        if (index >= 0 && index < modes.size()) startAssist(modes.at(index));
        return true;
    }
    return false;
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

void ResultDialog::setAssistEnabled(bool on)
{
    m_wantAssist = on;
    if (m_askRow) m_askRow->setVisible(on);
    if (m_rewriteRow) m_rewriteRow->setVisible(on);
    if (!on && m_question) m_question->clear();
    if (!on && m_openAi) m_openAi->hide();
}

void ResultDialog::focusQuestion()
{
    if (!m_question || !m_wantAssist) return;
    QPointer<ResultDialog> guard(this);
    const auto apply = [guard] {
        if (!guard || !guard->m_question || !guard->m_wantAssist || !guard->isVisible() || guard->m_busy) return;
        if (guard->m_openAi && guard->m_openAi->isVisible()) {
            if (!Settings::aiTranslateReady()) return;
            guard->m_openAi->hide();
            guard->m_setupNext = SetupNext::Continue;
        }
        guard->raise();
        guard->activateWindow();
        guard->m_question->setFocus(Qt::OtherFocusReason);
    };
    apply();
    QTimer::singleShot(0, this, apply);
}

void ResultDialog::showAiSettingsButton(SetupNext next)
{
    m_setupNext = next;
    if (!m_openAi) return;
    m_openAi->setEnabled(true);
    m_openAi->show();
}

void ResultDialog::showAiSetupNeeded(SetupNext next, const QString& status)
{
    setStatus(status.isEmpty() ? aiNeedText() : status, true);
    showAiSettingsButton(next);
    if (m_openAi && !m_busy) m_openAi->setFocus(Qt::OtherFocusReason);
}

void ResultDialog::noteSettingsReturned()
{
    if (!isActiveWindow() || !m_openAi || !m_openAi->isVisible()) return;
    if (!Settings::aiTranslateReady()) return;
    m_openAi->hide();
    if (m_busy) return;
    const SetupNext next = m_setupNext;
    m_setupNext = SetupNext::Continue;
    if (next == SetupNext::Ask && m_wantAssist) {
        setStatus(QCoreApplication::translate("QingYing", "在下方输入问题，按 Enter 提问。"));
        focusQuestion();
        return;
    }
    if (next == SetupNext::Rewrite)
        setStatus(QCoreApplication::translate("QingYing", "AI 已就绪，点一个改写方式即可。"));
    else
        setStatus(QCoreApplication::translate("QingYing", "AI 已就绪，可以继续。"));
}

QString ResultDialog::sourceForAssist() const
{
    if (m_restore->isVisible() && !m_original.isEmpty()) return m_original;
    return m_edit->toPlainText();
}

void ResultDialog::syncAssistButtons()
{
    const bool hasEditor = !m_edit->toPlainText().trimmed().isEmpty();
    const bool hasSource = !sourceForAssist().trimmed().isEmpty();
    if (m_auto) m_auto->setEnabled(!m_busy && hasEditor);
    if (m_toZh) m_toZh->setEnabled(!m_busy && hasEditor);
    if (m_toEn) m_toEn->setEnabled(!m_busy && hasEditor);
    if (m_ask) m_ask->setEnabled(!m_busy && hasSource);
    for (QPushButton* chip : m_rewriteChips) chip->setEnabled(!m_busy && hasSource);
    if (m_question) m_question->setEnabled(!m_busy);
}

void ResultDialog::startAssist(AskText::Mode mode)
{
    if (m_busy) return;
    const QString src = sourceForAssist();
    const QString question = mode == AskText::Mode::Ask && m_question ? m_question->text() : QString();
    if (src.trimmed().isEmpty()) {
        setStatus(mode == AskText::Mode::Ask ? QCoreApplication::translate("QingYing", "没有可提问的文字")
                                             : QCoreApplication::translate("QingYing", "没有可改写的文字"),
                  true);
        return;
    }
    if (!Settings::aiTranslateReady()) {
        showAiSetupNeeded(mode == AskText::Mode::Ask ? SetupNext::Ask : SetupNext::Rewrite);
        return;
    }
    if (mode == AskText::Mode::Ask && question.trimmed().isEmpty()) {
        setStatus(QCoreApplication::translate("QingYing", "请先输入问题"), true);
        focusQuestion();
        return;
    }
    if (m_openAi) m_openAi->hide();
    m_setupNext = SetupNext::Continue;
    if (!m_restore->isVisible()) m_original = src;
    const int gen = ++m_translateGen;
    setBusy(true);
    setStatus(mode == AskText::Mode::Ask ? QCoreApplication::translate("QingYing", "正在提问…")
                                         : QCoreApplication::translate("QingYing", "正在改写…"));
    QPointer<ResultDialog> guard(this);
    Translator::assist(mode, src, question, [guard, gen, mode](Translator::Result r) {
        if (!guard || gen != guard->m_translateGen) return;
        guard->setBusy(false);
        const QString title = mode == AskText::Mode::Ask ? QCoreApplication::translate("QingYing", "截图秒答")
                                                         : QCoreApplication::translate("QingYing", "改文案");
        if (!r.ok) {
            guard->setStatus(mode == AskText::Mode::Ask
                                 ? QCoreApplication::translate("QingYing", "提问失败：%1").arg(r.error)
                                 : QCoreApplication::translate("QingYing", "改写失败：%1").arg(r.error),
                             true);
            if (r.error != QCoreApplication::translate("QingYing", "已取消")) Notify::warn(title, r.error);
            return;
        }
        guard->m_edit->setPlainText(r.text);
        guard->m_edit->selectAll();
        if (!guard->m_original.isEmpty() && r.text == guard->m_original)
            guard->m_restore->hide();
        else {
            guard->m_restore->show();
            guard->m_restore->setEnabled(true);
        }
        guard->syncAssistButtons();
        if (r.fromCache) {
            guard->setStatus(mode == AskText::Mode::Ask
                                 ? QCoreApplication::translate("QingYing", "与刚才的回答相同，未再请求")
                                 : QCoreApplication::translate("QingYing", "与刚才的改写相同，未再请求"));
            return;
        }
        if (mode == AskText::Mode::Ask)
            guard->setStatus(QCoreApplication::translate("QingYing", "已回答 · 可复制，或点「还原原文」"));
        else
            guard->setStatus(QCoreApplication::translate("QingYing", "已按「%1」改写 · 可复制，或点「还原原文」").arg(AskText::modeTitle(mode)));
    });
}

void ResultDialog::setWorking(bool working) { setBusy(working); }

void ResultDialog::setBusy(bool busy)
{
    m_busy = busy;
    m_edit->setReadOnly(busy);
    syncAssistButtons();
    m_restore->setEnabled(!busy && !m_original.isEmpty());
    m_copy->setEnabled(!busy && !m_edit->toPlainText().isEmpty());
    m_open->setEnabled(!busy && !m_url.isEmpty());
    if (m_openAi) m_openAi->setEnabled(true);
}

void ResultDialog::translateSuggested()
{
    if (m_edit->toPlainText().trimmed().isEmpty()) return;
    if (!Settings::translateEnabled()) {
        const QString cur = m_status ? m_status->text() : QString();
        const QString extra = QCoreApplication::translate("QingYing", "联网翻译未开启，未自动翻译");
        setStatus(cur.isEmpty() ? extra : cur + QStringLiteral(" · ") + extra);
        return;
    }
    const bool toZh = Translator::suggestTarget(m_edit->toPlainText()) == Translator::Target::Chinese;
    startTranslate(toZh);
}

void ResultDialog::startTranslate(bool toChinese)
{
    // 按钮在 busy 时已禁用；这里再挡一次，避免重复点击发出第二次请求。
    if (m_busy) return;
    const QString src = m_edit->toPlainText();
    if (src.trimmed().isEmpty()) {
        setStatus(QCoreApplication::translate("QingYing", "无需翻译"));
        return;
    }
    if (!Settings::translateEnabled()) {
        setTranslateEnabled(m_wantTranslate);
        setStatus(QCoreApplication::translate("QingYing", "已在设置中关闭联网翻译"), true);
        return;
    }
    if (Settings::aiTranslateEnabled() && !Settings::aiTranslateReady() && !Settings::memoryFallback()) {
        showAiSetupNeeded(SetupNext::Continue, QCoreApplication::translate("QingYing", "请在设置中填写 AI 翻译的接口地址、密钥和模型，或打开 MyMemory 兜底"));
        return;
    }
    if (m_openAi) m_openAi->hide();
    if (m_original.isEmpty()) m_original = src;
    const int gen = ++m_translateGen;
    setBusy(true);
    setStatus(Settings::aiTranslateReady() ? QCoreApplication::translate("QingYing", "正在用 AI 翻译…")
                                           : QCoreApplication::translate("QingYing", "正在翻译…"));
    QPointer<ResultDialog> guard(this);
    const auto target = toChinese ? Translator::Target::Chinese : Translator::Target::English;
    Translator::translate(src, target, [guard, toChinese, gen, src](Translator::Result r) {
        if (!guard || gen != guard->m_translateGen) return;
        guard->setBusy(false);
        if (!r.ok) {
            const bool missingAi = r.error == aiNeedText()
                || r.error == QCoreApplication::translate("QingYing", "请在设置中填写 AI 翻译的接口地址、密钥和模型，或打开 MyMemory 兜底");
            if (missingAi) {
                guard->showAiSetupNeeded(SetupNext::Continue, r.error);
                return;
            }
            guard->setStatus(QCoreApplication::translate("QingYing", "翻译失败：%1").arg(r.error), true);
            return;
        }
        const QString lang = toChinese ? QCoreApplication::translate("QingYing", "中文")
                                       : QCoreApplication::translate("QingYing", "英文");
        if (r.local == TranslateText::ReuseKind::NoProse) {
            if (guard->m_original == src) guard->m_original.clear();
            guard->setStatus(QCoreApplication::translate("QingYing", "无需翻译"));
            return;
        }
        if (r.local == TranslateText::ReuseKind::Already) {
            if (guard->m_original == src) guard->m_original.clear();
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
    ++m_translateGen;
    Translator::cancelInFlight();
    Translator::forgetRecall();
    if (m_busy) setBusy(false);
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
