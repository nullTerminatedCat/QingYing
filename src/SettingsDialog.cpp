#include <QCoreApplication>
#include "Theme.h"
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
#include <QPlainTextEdit>
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
    setWindowTitle(QCoreApplication::translate("QingYing", "青影 设置"));
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setMinimumWidth(480);
    Theme::setSheet(this, QStringLiteral(R"(
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
        QPlainTextEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px; }
        QSpinBox { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px 8px; min-width: 90px; }
        QCheckBox { font-size: 13px; spacing: 8px; }
        QCheckBox::indicator { width: 34px; height: 20px; border-radius: 10px; background: #D5D7DE; }
        QCheckBox::indicator:checked { background: #4F6BFF; }
    )"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(14);
    auto* title = new QLabel(QCoreApplication::translate("QingYing", "设置"), this);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);

    QVBoxLayout* b = nullptr;
    root->addWidget(card(this, QCoreApplication::translate("QingYing", "快捷键"), b));
    auto* h = row(b, QCoreApplication::translate("QingYing", "截图"));
    m_hotkey = new QKeySequenceEdit(Settings::hotkey(), this);
    m_hotkey->setMaximumSequenceLength(1);
    m_hotkey->setClearButtonEnabled(true);
    h->addWidget(m_hotkey, 1);
    auto* reset = new QPushButton(QCoreApplication::translate("QingYing", "恢复默认"), this);
    h->addWidget(reset);
    m_hotkeyHint = new QLabel(QCoreApplication::translate("QingYing", "点击输入框后直接按下新的组合键，例如 Ctrl+Alt+A"), this);
    m_hotkeyHint->setObjectName(QStringLiteral("hint"));
    b->addWidget(m_hotkeyHint);
    connect(reset, &QPushButton::clicked, this, [this] { m_hotkey->setKeySequence(Settings::defaultHotkey()); });
    connect(m_hotkey, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence& k) {
        unsigned m = 0, v = 0;
        const bool ok = k.isEmpty() || (HotkeyManager::toNative(k, m, v) && m != 0);
        m_hotkeyHint->setObjectName(ok ? QStringLiteral("hint") : QStringLiteral("warn"));
        m_hotkeyHint->setText(ok ? QCoreApplication::translate("QingYing", "点击输入框后直接按下新的组合键，例如 Ctrl+Alt+A")
                                 : QCoreApplication::translate("QingYing", "需要包含 Ctrl / Alt / Shift / Win 之一，并使用字母、数字或功能键"));
        m_hotkeyHint->style()->unpolish(m_hotkeyHint);
        m_hotkeyHint->style()->polish(m_hotkeyHint);
    });

    root->addWidget(card(this, QCoreApplication::translate("QingYing", "保存"), b));
    h = row(b, QCoreApplication::translate("QingYing", "保存位置"));
    m_dir = new QLineEdit(QDir::toNativeSeparators(Settings::saveDir()), this);
    h->addWidget(m_dir, 1);
    auto* browse = new QPushButton(QCoreApplication::translate("QingYing", "浏览…"), this);
    h->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, QCoreApplication::translate("QingYing", "选择保存位置"), m_dir->text());
        if (!d.isEmpty()) m_dir->setText(QDir::toNativeSeparators(d));
    });
    h = row(b, QCoreApplication::translate("QingYing", "文件名"));
    m_pattern = new QLineEdit(Settings::filePattern(), this);
    h->addWidget(m_pattern, 1);
    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("hint"));
    b->addWidget(m_preview);
    h = row(b, QCoreApplication::translate("QingYing", "格式"));
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

    root->addWidget(card(this, QCoreApplication::translate("QingYing", "截图完成"), b));
    h = row(b, QCoreApplication::translate("QingYing", "默认动作"));
    m_action = new QButtonGroup(this);
    const QStringList keys = {QStringLiteral("copy"), QStringLiteral("save"), QStringLiteral("pin")};
    const QStringList names = {QCoreApplication::translate("QingYing", "复制"), QCoreApplication::translate("QingYing", "保存"), QCoreApplication::translate("QingYing", "钉图")};
    for (int i = 0; i < 3; ++i) {
        auto* s = new QPushButton(names[i], this);
        s->setObjectName(QStringLiteral("seg"));
        s->setCheckable(true);
        s->setChecked(Settings::defaultAction() == keys[i]);
        m_action->addButton(s, i);
        h->addWidget(s);
    }
    h->addStretch();
    auto* actHint = new QLabel(QCoreApplication::translate("QingYing", "按 Enter、双击选区或点击 ✓ 时执行"), this);
    actHint->setObjectName(QStringLiteral("hint"));
    b->addWidget(actHint);
    h = row(b, QCoreApplication::translate("QingYing", "取色格式"));
    m_colorFmt = new QComboBox(this);
    m_colorFmt->addItem(QCoreApplication::translate("QingYing", "#4F6BFF（HEX 大写）"), QStringLiteral("hex"));
    m_colorFmt->addItem(QCoreApplication::translate("QingYing", "#4f6bff（HEX 小写）"), QStringLiteral("hexlower"));
    m_colorFmt->addItem(QStringLiteral("rgb(79, 107, 255)"), QStringLiteral("rgb"));
    m_colorFmt->addItem(QStringLiteral("79, 107, 255"), QStringLiteral("rgbplain"));
    m_colorFmt->setCurrentIndex(qMax(0, m_colorFmt->findData(Settings::colorFormat())));
    h->addWidget(m_colorFmt);
    h->addStretch();
    h = row(b, QCoreApplication::translate("QingYing", "历史记录"));
    m_history = new QSpinBox(this);
    m_history->setRange(0, 500);
    m_history->setValue(Settings::historySize());
    m_history->setSuffix(QCoreApplication::translate("QingYing", " 张"));
    m_history->setSpecialValueText(QCoreApplication::translate("QingYing", "不保留"));
    h->addWidget(m_history);
    h->addStretch();

    root->addWidget(card(this, QCoreApplication::translate("QingYing", "录屏"), b));
    h = row(b, QCoreApplication::translate("QingYing", "帧率"));
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
    m_recCursor = new QCheckBox(QCoreApplication::translate("QingYing", "录制鼠标指针"), this);
    m_recCursor->setChecked(Settings::recordCursor());
    b->addWidget(m_recCursor);

    root->addWidget(card(this, QCoreApplication::translate("QingYing", "上传"), b));
    const Uploads::Config up = Settings::uploadConfig();
    m_upEnabled = new QCheckBox(QCoreApplication::translate("QingYing", "启用上传（截图工具条与钉图菜单出现“上传”）"), this);
    m_upEnabled->setChecked(Settings::uploadEnabled());
    b->addWidget(m_upEnabled);
    h = row(b, QCoreApplication::translate("QingYing", "服务"));
    m_upProvider = new QComboBox(this);
    m_upProvider->addItem(QCoreApplication::translate("QingYing", "自定义 HTTP（multipart POST）"), QStringLiteral("custom"));
    m_upProvider->addItem(QStringLiteral("SM.MS"), QStringLiteral("smms"));
    m_upProvider->addItem(QStringLiteral("Imgur"), QStringLiteral("imgur"));
    m_upProvider->setCurrentIndex(qMax(0, m_upProvider->findData(up.provider)));
    h->addWidget(m_upProvider, 1);
    m_upCustom = new QWidget(this);
    auto* cl = new QVBoxLayout(m_upCustom);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(10);
    auto* cr = row(cl, QCoreApplication::translate("QingYing", "上传地址"));
    m_upUrl = new QLineEdit(up.url, this);
    m_upUrl->setPlaceholderText(QStringLiteral("https://example.com/upload"));
    cr->addWidget(m_upUrl, 1);
    cr = row(cl, QCoreApplication::translate("QingYing", "文件字段"));
    m_upField = new QLineEdit(up.field, this);
    cr->addWidget(m_upField, 1);
    cr = row(cl, QCoreApplication::translate("QingYing", "链接路径"));
    m_upPath = new QLineEdit(up.jsonPath, this);
    m_upPath->setPlaceholderText(QCoreApplication::translate("QingYing", "返回 JSON 中链接的位置，如 data.url"));
    cr->addWidget(m_upPath, 1);
    cr = row(cl, QCoreApplication::translate("QingYing", "请求头"));
    m_upHeaders = new QPlainTextEdit(this);
    m_upHeaders->setPlaceholderText(QCoreApplication::translate("QingYing", "每行一个，如 Authorization: Bearer xxx"));
    m_upHeaders->setFixedHeight(64);
    cr->addWidget(m_upHeaders, 1);
    b->addWidget(m_upCustom);
    m_upPreset = new QWidget(this);
    auto* pl = new QVBoxLayout(m_upPreset);
    pl->setContentsMargins(0, 0, 0, 0);
    auto* pr = row(pl, QStringLiteral("Token"));
    m_upToken = new QLineEdit(this);
    m_upToken->setEchoMode(QLineEdit::Password);
    pr->addWidget(m_upToken, 1);
    b->addWidget(m_upPreset);
    if (up.provider == QLatin1String("custom")) m_upHeaders->setPlainText(up.secret);
    else m_upToken->setText(up.secret);
    auto* secretHint = new QLabel(QCoreApplication::translate("QingYing", "密钥使用 Windows DPAPI 加密保存，仅当前用户可解密"), this);
    secretHint->setObjectName(QStringLiteral("hint"));
    b->addWidget(secretHint);
    connect(m_upProvider, &QComboBox::currentIndexChanged, this, &SettingsDialog::updateUploadUi);
    updateUploadUi();

    root->addWidget(card(this, QCoreApplication::translate("QingYing", "常规"), b));
    h = row(b, QCoreApplication::translate("QingYing", "外观"));
    m_theme = new QButtonGroup(this);
    const QStringList themeKeys = {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
    const QStringList themeNames = {QCoreApplication::translate("QingYing", "跟随系统"), QCoreApplication::translate("QingYing", "浅色"), QCoreApplication::translate("QingYing", "深色")};
    for (int i = 0; i < 3; ++i) {
        auto* s = new QPushButton(themeNames[i], this);
        s->setObjectName(QStringLiteral("seg"));
        s->setCheckable(true);
        s->setChecked(Settings::theme() == themeKeys[i]);
        m_theme->addButton(s, i);
        h->addWidget(s);
    }
    h->addStretch();
    h = row(b, QCoreApplication::translate("QingYing", "语言"));
    m_lang = new QComboBox(this);
    m_lang->addItem(QCoreApplication::translate("QingYing", "跟随系统"), QStringLiteral("system"));
    m_lang->addItem(QString::fromUtf8("\u7b80\u4f53\u4e2d\u6587"), QStringLiteral("zh_CN"));
    m_lang->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    m_lang->setCurrentIndex(qMax(0, m_lang->findData(Settings::language())));
    h->addWidget(m_lang);
    h->addStretch();
    m_autostart = new QCheckBox(QCoreApplication::translate("QingYing", "开机时自动启动青影"), this);
    m_autostart->setChecked(Settings::autostart());
    b->addWidget(m_autostart);
    m_autoUpdate = new QCheckBox(QCoreApplication::translate("QingYing", "启动时自动检查更新（每天最多一次）"), this);
    m_autoUpdate->setChecked(Settings::autoUpdate());
    b->addWidget(m_autoUpdate);

    root->addStretch();
    auto* btns = new QHBoxLayout;
    btns->addStretch();
    auto* cancel = new QPushButton(QCoreApplication::translate("QingYing", "取消"), this);
    auto* ok = new QPushButton(QCoreApplication::translate("QingYing", "保存"), this);
    ok->setObjectName(QStringLiteral("primary"));
    ok->setDefault(true);
    btns->addWidget(cancel);
    btns->addWidget(ok);
    root->addLayout(btns);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &SettingsDialog::save);
    updatePreview();
}

void SettingsDialog::updateUploadUi()
{
    const bool custom = m_upProvider->currentData().toString() == QLatin1String("custom");
    m_upCustom->setVisible(custom);
    m_upPreset->setVisible(!custom);
    m_upToken->setPlaceholderText(Uploads::create({m_upProvider->currentData().toString()})->secretHint());
}

void SettingsDialog::updatePreview()
{
    const QString ext = m_jpg->isChecked() ? QStringLiteral(".jpg") : QStringLiteral(".png");
    m_preview->setText(QCoreApplication::translate("QingYing", "预览：%1%2    {} 内为日期格式，如 {yyyyMMdd_HHmmss}")
                           .arg(Settings::expandPattern(m_pattern->text()), ext));
}

void SettingsDialog::save()
{
    QKeySequence k = m_hotkey->keySequence();
    if (k.isEmpty()) k = Settings::defaultHotkey();
    unsigned m = 0, v = 0;
    if (!HotkeyManager::toNative(k, m, v) || m == 0) {
        QMessageBox::warning(this, QCoreApplication::translate("QingYing", "快捷键无效"),
                             QCoreApplication::translate("QingYing", "快捷键需要包含 Ctrl / Alt / Shift / Win 之一，并使用字母、数字或功能键。"));
        return;
    }
    if (!m_apply(k)) {
        QMessageBox::warning(this, QCoreApplication::translate("QingYing", "快捷键冲突"),
                             QCoreApplication::translate("QingYing", "快捷键 %1 已被其他程序占用，请换一个组合。").arg(k.toString(QKeySequence::NativeText)));
        return;
    }
    Settings::setHotkey(k);
    Settings::setSaveDir(QDir::fromNativeSeparators(m_dir->text().trimmed()));
    Settings::setFilePattern(m_pattern->text().trimmed().isEmpty() ? QStringLiteral("QingYing_{yyyyMMdd_HHmmss}")
                                                                    : m_pattern->text().trimmed());
    Settings::setFormat(m_jpg->isChecked() ? QStringLiteral("jpg") : QStringLiteral("png"));
    Settings::setAutostart(m_autostart->isChecked());
    Settings::setAutoUpdate(m_autoUpdate->isChecked());
    Settings::setUploadEnabled(m_upEnabled->isChecked());
    Uploads::Config up;
    up.provider = m_upProvider->currentData().toString();
    up.url = m_upUrl->text().trimmed();
    up.field = m_upField->text().trimmed();
    up.jsonPath = m_upPath->text().trimmed();
    up.secret = up.provider == QLatin1String("custom") ? m_upHeaders->toPlainText() : m_upToken->text();
    Settings::setUploadConfig(up);
    static const char* keys[3] = {"copy", "save", "pin"};
    Settings::setDefaultAction(QString::fromLatin1(keys[qBound(0, m_action->checkedId(), 2)]));
    Settings::setHistorySize(m_history->value());
    static const char* themeKeys[3] = {"system", "light", "dark"};
    const QString newTheme = QString::fromLatin1(themeKeys[qBound(0, m_theme->checkedId(), 2)]);
    if (newTheme != Settings::theme()) {
        Settings::setTheme(newTheme);
        Theme::apply();
    }
    const QString newLang = m_lang->currentData().toString();
    if (newLang != Settings::language()) {
        const QString before = Settings::effectiveLanguage();
        Settings::setLanguage(newLang);
        if (Settings::effectiveLanguage() != before)
            QMessageBox::information(this, QCoreApplication::translate("QingYing", "青影 设置"),
                                     QCoreApplication::translate("QingYing", "语言设置将在重启青影后生效。"));
    }
    Settings::setColorFormat(m_colorFmt->currentData().toString());
    Settings::setRecordFps(m_fps->checkedId() > 0 ? m_fps->checkedId() : 15);
    Settings::setRecordCursor(m_recCursor->isChecked());
    accept();
}
