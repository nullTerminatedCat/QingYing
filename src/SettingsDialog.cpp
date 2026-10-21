#include "SettingsDialog.h"
#include "HotkeyManager.h"
#include "Icons.h"
#include "Settings.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QFrame* card(QWidget* parent, const QString& title, QVBoxLayout*& body)
{
    auto* f = new QFrame(parent);
    f->setObjectName(QStringLiteral("card"));
    body = new QVBoxLayout(f);
    body->setContentsMargins(16, 14, 16, 14);
    body->setSpacing(10);
    auto* t = new QLabel(title, f);
    t->setObjectName(QStringLiteral("cardTitle"));
    body->addWidget(t);
    return f;
}

QHBoxLayout* row(QVBoxLayout* body, const QString& label)
{
    auto* h = new QHBoxLayout;
    h->setSpacing(8);
    auto* l = new QLabel(label);
    l->setMinimumWidth(72);
    h->addWidget(l);
    body->addLayout(h);
    return h;
}
}

SettingsDialog::SettingsDialog(std::function<bool(const QKeySequence&)> apply, QWidget* parent)
    : QDialog(parent), m_apply(std::move(apply))
{
    setWindowTitle(QStringLiteral("青影 设置"));
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(480);
    setStyleSheet(QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel { color: #1C1C1E; font-size: 13px; }
        QLabel#title { font-size: 20px; font-weight: 600; }
        QLabel#cardTitle { font-size: 12px; color: #6E6E73; font-weight: 600; }
        QLabel#hint { font-size: 12px; color: #A1A1A6; }
        QLabel#warn { font-size: 12px; color: #FF453A; }
        QFrame#card { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; }
        QLineEdit, QKeySequenceEdit QLineEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,30);
            border-radius: 8px; padding: 5px 8px; font-size: 13px; selection-background-color: #4F6BFF; }
        QLineEdit:focus { border: 1px solid #4F6BFF; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 10px;
            padding: 6px 16px; font-size: 13px; color: #1C1C1E; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
        QPushButton#primary:hover { background: #3D57E8; }
        QPushButton#seg { border-radius: 8px; padding: 5px 18px; }
        QPushButton#seg:checked { background: #4F6BFF; color: white; border: 1px solid #4F6BFF; }
        QComboBox { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px 8px; min-width: 160px; }
        QSpinBox { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px 8px; min-width: 90px; }
        QCheckBox { font-size: 13px; spacing: 8px; }
        QCheckBox::indicator { width: 34px; height: 20px; border-radius: 10px; background: #D5D7DE; }
        QCheckBox::indicator:checked { background: #4F6BFF; }
    )"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    auto* title = new QLabel(QStringLiteral("设置"), this);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    QVBoxLayout* b = nullptr;
    root->addWidget(card(this, QStringLiteral("快捷键"), b));
    auto* h = row(b, QStringLiteral("截图"));
    m_hotkey = new QKeySequenceEdit(Settings::hotkey(), this);
    m_hotkey->setMaximumSequenceLength(1);
    m_hotkey->setClearButtonEnabled(true);
    h->addWidget(m_hotkey, 1);
    auto* reset = new QPushButton(QStringLiteral("恢复默认"), this);
    h->addWidget(reset);
    m_hotkeyHint = new QLabel(QStringLiteral("点击输入框后直接按下新的组合键，例如 Ctrl+Alt+A"), this);
    m_hotkeyHint->setObjectName(QStringLiteral("hint"));
    b->addWidget(m_hotkeyHint);
    connect(reset, &QPushButton::clicked, this, [this] { m_hotkey->setKeySequence(Settings::defaultHotkey()); });
    connect(m_hotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence& k) {
        unsigned m = 0, v = 0;
        const bool ok = k.isEmpty() || (HotkeyManager::toNative(k, m, v) && m != 0);
        m_hotkeyHint->setObjectName(ok ? QStringLiteral("hint") : QStringLiteral("warn"));
        m_hotkeyHint->setText(ok ? QStringLiteral("点击输入框后直接按下新的组合键，例如 Ctrl+Alt+A")
                                 : QStringLiteral("需要包含 Ctrl / Alt / Shift / Win 之一，并使用字母、数字或功能键"));
        m_hotkeyHint->style()->unpolish(m_hotkeyHint);
        m_hotkeyHint->style()->polish(m_hotkeyHint);
    });

    root->addWidget(card(this, QStringLiteral("保存"), b));
    h = row(b, QStringLiteral("保存位置"));
    m_dir = new QLineEdit(QDir::toNativeSeparators(Settings::saveDir()), this);
    h->addWidget(m_dir, 1);
    auto* browse = new QPushButton(QStringLiteral("浏览…"), this);
    h->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("选择保存位置"), m_dir->text());
        if (!d.isEmpty()) m_dir->setText(QDir::toNativeSeparators(d));
    });
    h = row(b, QStringLiteral("文件名"));
    m_pattern = new QLineEdit(Settings::filePattern(), this);
    h->addWidget(m_pattern, 1);
    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("hint"));
    b->addWidget(m_preview);
    h = row(b, QStringLiteral("格式"));
    m_png = new QPushButton(QStringLiteral("PNG"), this);
    m_jpg = new QPushButton(QStringLiteral("JPG"), this);
    for (QPushButton* s : {m_png, m_jpg}) {
        s->setObjectName(QStringLiteral("seg"));
        s->setCheckable(true);
        h->addWidget(s);
    }
    h->addStretch();
    auto* grp = new QButtonGroup(this);
    grp->addButton(m_png);
    grp->addButton(m_jpg);
    (Settings::format() == QLatin1String("jpg") ? m_jpg : m_png)->setChecked(true);
    connect(m_pattern, &QLineEdit::textChanged, this, &SettingsDialog::updatePreview);
    connect(grp, &QButtonGroup::buttonToggled, this, &SettingsDialog::updatePreview);

    root->addWidget(card(this, QStringLiteral("截图完成"), b));
    h = row(b, QStringLiteral("默认动作"));
    m_action = new QButtonGroup(this);
    const QStringList keys = {QStringLiteral("copy"), QStringLiteral("save"), QStringLiteral("pin")};
    const QStringList names = {QStringLiteral("复制"), QStringLiteral("保存"), QStringLiteral("钉图")};
    for (int i = 0; i < 3; ++i) {
        auto* s = new QPushButton(names[i], this);
        s->setObjectName(QStringLiteral("seg"));
        s->setCheckable(true);
        s->setChecked(Settings::defaultAction() == keys[i]);
        m_action->addButton(s, i);
        h->addWidget(s);
    }
    h->addStretch();
    auto* actHint = new QLabel(QStringLiteral("按 Enter、双击选区或点击 ✓ 时执行"), this);
    actHint->setObjectName(QStringLiteral("hint"));
    b->addWidget(actHint);
    h = row(b, QStringLiteral("取色格式"));
    m_colorFmt = new QComboBox(this);
    m_colorFmt->addItem(QStringLiteral("#4F6BFF（HEX 大写）"), QStringLiteral("hex"));
    m_colorFmt->addItem(QStringLiteral("#4f6bff（HEX 小写）"), QStringLiteral("hexlower"));
    m_colorFmt->addItem(QStringLiteral("rgb(79, 107, 255)"), QStringLiteral("rgb"));
    m_colorFmt->addItem(QStringLiteral("79, 107, 255"), QStringLiteral("rgbplain"));
    m_colorFmt->setCurrentIndex(qMax(0, m_colorFmt->findData(Settings::colorFormat())));
    h->addWidget(m_colorFmt);
    h->addStretch();
    h = row(b, QStringLiteral("历史记录"));
    m_history = new QSpinBox(this);
    m_history->setRange(0, 500);
    m_history->setValue(Settings::historySize());
    m_history->setSuffix(QStringLiteral(" 张"));
    m_history->setSpecialValueText(QStringLiteral("不保留"));
    h->addWidget(m_history);
    h->addStretch();

    root->addWidget(card(this, QStringLiteral("录屏"), b));
    h = row(b, QStringLiteral("帧率"));
    m_fps = new QButtonGroup(this);
    for (int f : {10, 15, 30}) {
        auto* s = new QPushButton(QStringLiteral("%1 fps").arg(f), this);
        s->setObjectName(QStringLiteral("seg"));
        s->setCheckable(true);
        s->setChecked(Settings::recordFps() == f);
        m_fps->addButton(s, f);
        h->addWidget(s);
    }
    h->addStretch();
    m_recCursor = new QCheckBox(QStringLiteral("录制鼠标指针"), this);
    m_recCursor->setChecked(Settings::recordCursor());
    b->addWidget(m_recCursor);

    root->addWidget(card(this, QStringLiteral("常规"), b));
    m_autostart = new QCheckBox(QStringLiteral("开机时自动启动青影"), this);
    m_autostart->setChecked(Settings::autostart());
    b->addWidget(m_autostart);

    root->addStretch();
    auto* btns = new QHBoxLayout;
    btns->addStretch();
    auto* cancel = new QPushButton(QStringLiteral("取消"), this);
    auto* ok = new QPushButton(QStringLiteral("保存"), this);
    ok->setObjectName(QStringLiteral("primary"));
    ok->setDefault(true);
    btns->addWidget(cancel);
    btns->addWidget(ok);
    root->addLayout(btns);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::save);
    updatePreview();
}

void SettingsDialog::updatePreview()
{
    const QString ext = m_jpg->isChecked() ? QStringLiteral(".jpg") : QStringLiteral(".png");
    m_preview->setText(QStringLiteral("预览：%1%2    {} 内为日期格式，如 {yyyyMMdd_HHmmss}")
                           .arg(Settings::expandPattern(m_pattern->text()), ext));
}

void SettingsDialog::save()
{
    QKeySequence k = m_hotkey->keySequence();
    if (k.isEmpty()) k = Settings::defaultHotkey();
    unsigned m = 0, v = 0;
    if (!HotkeyManager::toNative(k, m, v) || m == 0) {
        QMessageBox::warning(this, QStringLiteral("快捷键无效"),
                             QStringLiteral("快捷键需要包含 Ctrl / Alt / Shift / Win 之一，并使用字母、数字或功能键。"));
        return;
    }
    if (!m_apply(k)) {
        QMessageBox::warning(this, QStringLiteral("快捷键冲突"),
                             QStringLiteral("快捷键 %1 已被其他程序占用，请换一个组合。").arg(k.toString(QKeySequence::NativeText)));
        return;
    }
    Settings::setHotkey(k);
    Settings::setSaveDir(QDir::fromNativeSeparators(m_dir->text().trimmed()));
    Settings::setFilePattern(m_pattern->text().trimmed().isEmpty() ? QStringLiteral("QingYing_{yyyyMMdd_HHmmss}")
                                                                    : m_pattern->text().trimmed());
    Settings::setFormat(m_jpg->isChecked() ? QStringLiteral("jpg") : QStringLiteral("png"));
    Settings::setAutostart(m_autostart->isChecked());
    static const char* keys[3] = {"copy", "save", "pin"};
    Settings::setDefaultAction(QString::fromLatin1(keys[qBound(0, m_action->checkedId(), 2)]));
    Settings::setHistorySize(m_history->value());
    Settings::setColorFormat(m_colorFmt->currentData().toString());
    Settings::setRecordFps(m_fps->checkedId() > 0 ? m_fps->checkedId() : 15);
    Settings::setRecordCursor(m_recCursor->isChecked());
    accept();
}
