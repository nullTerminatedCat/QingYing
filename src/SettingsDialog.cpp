#include "SettingsDialog.h"
#include "Diagnostics.h"
#include "HotkeyManager.h"
#include "Icons.h"
#include "Language.h"
#include "Settings.h"
#include "SettingsWidgets.h"
#include "Style.h"
#include "Theme.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const char* kRepoUrl = "https://github.com/nullTerminatedCat/QingYing";

// 一张卡片：返回放行的布局；行之间自动加分隔线
struct Card {
    QFrame* frame;
    QVBoxLayout* lay;
    int rows = 0;
};

Card addCard(QVBoxLayout* page, const QString& title)
{
    if (!title.isEmpty()) {
        auto* t = new QLabel(title);
        t->setObjectName(QStringLiteral("section"));
        page->addWidget(t);
    }
    auto* f = new QFrame;
    f->setObjectName(QStringLiteral("card"));
    auto* l = new QVBoxLayout(f);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    page->addWidget(f);
    page->addSpacing(14);
    return {f, l};
}

// 一行：左侧标题 + 一行说明，右侧控件；wide 时控件放到下方占满宽度
QWidget* addRow(Card& c, const QString& label, const QString& hint, QWidget* control, bool wide = false, QLabel** hintOut = nullptr)
{
    if (c.rows++ > 0) {
        auto* sep = new QFrame;
        sep->setObjectName(QStringLiteral("sep"));
        sep->setFixedHeight(1);
        auto* sl = new QHBoxLayout;
        sl->setContentsMargins(16, 0, 0, 0);
        sl->addWidget(sep);
        c.lay->addLayout(sl);
    }
    auto* row = new QWidget;
    row->setMinimumHeight(56);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* l = new QLabel(label);
    l->setObjectName(QStringLiteral("rowLabel"));
    text->addWidget(l);
    QLabel* h = nullptr;
    if (!hint.isEmpty() || hintOut) {
        h = new QLabel(hint);
        h->setObjectName(QStringLiteral("rowHint"));
        h->setWordWrap(true);
        text->addWidget(h);
    }
    if (hintOut) *hintOut = h;
    if (wide) {
        auto* v = new QVBoxLayout(row);
        v->setContentsMargins(16, 12, 16, 12);
        v->setSpacing(8);
        v->addLayout(text);
        if (control) v->addWidget(control);
    } else {
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(16, 10, 16, 10);
        hl->setSpacing(16);
        hl->addLayout(text, 1);
        if (control) hl->addWidget(control, 0, Qt::AlignVCenter);
    }
    c.lay->addWidget(row);
    return row;
}

QWidget* hbox(std::initializer_list<QWidget*> ws, int stretchIndex = 0)
{
    auto* w = new QWidget;
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    int i = 0;
    for (QWidget* x : ws) h->addWidget(x, i++ == stretchIndex ? 1 : 0);
    return w;
}

QToolButton* iconButton(Icon icon, const QString& tip)
{
    auto* b = new QToolButton;
    b->setObjectName(QStringLiteral("iconBtn"));
    b->setIcon(menuIcon(icon));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

ToggleSwitch* toggle(bool on, std::function<void(bool)> f)
{
    auto* t = new ToggleSwitch;
    t->setChecked(on);
    QObject::connect(t, &QAbstractButton::toggled, t, std::move(f));
    return t;
}

const QStringList kThemes = {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};
const QStringList kActions = {QStringLiteral("copy"), QStringLiteral("save"), QStringLiteral("pin")};
const int kFps[3] = {10, 15, 30};
}

SettingsDialog::SettingsDialog(std::function<bool(const QKeySequence&)> apply, QWidget* parent)
    : QDialog(parent), m_apply(std::move(apply))
{
    setWindowTitle(QCoreApplication::translate("QingYing", "青影 设置"));
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setSizeGripEnabled(false);
    setMinimumSize(700, 480);
    Theme::setSheet(this, QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel { color: #1C1C1E; font-size: 13px; }
        QFrame#side { background: #FFFFFF; border: none; border-right: 1px solid rgba(0,0,0,15); }
        QLabel#brand { font-size: 15px; font-weight: 600; }
        QListWidget#nav { background: transparent; border: none; outline: 0; font-size: 13px; }
        QListWidget#nav::item { height: 36px; padding-left: 8px; border-radius: 8px; color: #1C1C1E; margin: 1px 0; }
        QListWidget#nav::item:hover { background: #F0F2FA; }
        QListWidget#nav::item:selected { background: rgba(79,107,255,30); color: #4F6BFF; }
        QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
        QLabel#pageTitle { font-size: 20px; font-weight: 600; }
        QLabel#pageSub { font-size: 12px; color: #6E6E73; }
        QLabel#section { font-size: 12px; color: #6E6E73; font-weight: 600; padding: 0 0 6px 4px; }
        QFrame#card { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; }
        QFrame#sep { background: rgba(0,0,0,12); border: none; }
        QLabel#rowLabel { font-size: 13px; }
        QLabel#rowHint { font-size: 12px; color: #6E6E73; }
        QLabel#rowHint[state="warn"] { color: #FF453A; }
        QLabel#rowHint[state="ok"] { color: #2FA84F; }
        QLabel#preview { font-size: 12px; color: #6E6E73; background: #F0F2FA; border-radius: 6px; padding: 6px 8px; }
        QLineEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 5px 8px;
            font-size: 13px; min-height: 20px; selection-background-color: #4F6BFF; }
        QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid #4F6BFF; }
        QLineEdit[state="warn"] { border: 1px solid #FF453A; }
        QPlainTextEdit { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px; font-size: 13px; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px;
            padding: 5px 14px; font-size: 13px; color: #1C1C1E; min-height: 20px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: 1px solid #4F6BFF; color: white; font-weight: 600; }
        QPushButton#primary:hover { background: #3D57E8; }
        QPushButton#hotkey { font-weight: 600; min-width: 160px; }
        QPushButton#hotkey[recording="true"] { border: 1px solid #4F6BFF; color: #4F6BFF; background: rgba(79,107,255,18); }
        QToolButton#iconBtn { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 5px; }
        QToolButton#iconBtn:hover { background: #F0F2FA; }
        QFrame#seg { background: #F0F2FA; border-radius: 9px; }
        QPushButton#segBtn { background: transparent; border: none; border-radius: 7px; padding: 4px 14px; color: #6E6E73; min-height: 20px; }
        QPushButton#segBtn:hover { color: #1C1C1E; }
        QPushButton#segBtn:checked { background: #FFFFFF; color: #4F6BFF; border: 1px solid rgba(0,0,0,15); }
        QComboBox { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px 8px; min-width: 190px; min-height: 22px; }
        QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 22px; border: none; }
        QComboBox::down-arrow { image: url(:/ui/chevron-down.png); width: 12px; height: 12px; }
        QSpinBox { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 8px; padding: 4px 26px 4px 8px; min-width: 100px; min-height: 22px; }
        QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 22px; border: none; background: transparent; }
        QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 8px; }
        QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 8px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: rgba(79,107,255,25); }
        QSpinBox::up-arrow { image: url(:/ui/chevron-up.png); width: 10px; height: 10px; }
        QSpinBox::down-arrow { image: url(:/ui/chevron-down.png); width: 10px; height: 10px; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px 4px 0; }
        QScrollBar::handle:vertical { background: rgba(0,0,0,45); border-radius: 4px; min-height: 32px; }
        QScrollBar::handle:vertical:hover { background: rgba(0,0,0,80); }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
        QLabel#aboutName { font-size: 18px; font-weight: 600; }
    )"));

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* side = new QFrame(this);
    side->setObjectName(QStringLiteral("side"));
    side->setFixedWidth(196);
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(12, 18, 12, 12);
    sl->setSpacing(10);
    auto* brandIcon = new QLabel(side);
    brandIcon->setPixmap(appIcon().pixmap(24, 24));
    auto* brand = new QLabel(QCoreApplication::translate("QingYing", "青影 设置"), side);
    brand->setObjectName(QStringLiteral("brand"));
    auto* bl = new QHBoxLayout;
    bl->setContentsMargins(8, 0, 0, 6);
    bl->setSpacing(8);
    bl->addWidget(brandIcon);
    bl->addWidget(brand, 1);
    sl->addLayout(bl);
    m_nav = new QListWidget(side);
    m_nav->setObjectName(QStringLiteral("nav"));
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setTextElideMode(Qt::ElideRight);
    sl->addWidget(m_nav, 1);
    root->addWidget(side);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack, 1);

    const std::pair<Icon, QString> nav[] = {
        {Icon::Settings, QCoreApplication::translate("QingYing", "常规")},  {Icon::Capture, QCoreApplication::translate("QingYing", "截图")},       {Icon::Pen, QCoreApplication::translate("QingYing", "标注")},
        {Icon::Record, QCoreApplication::translate("QingYing", "录屏")},    {Icon::Upload, QCoreApplication::translate("QingYing", "上传")},        {Icon::Palette, QCoreApplication::translate("QingYing", "外观与语言")},
        {Icon::Info, QCoreApplication::translate("QingYing", "关于与更新")},
    };
    for (const auto& [icon, name] : nav) m_nav->addItem(new QListWidgetItem(menuIcon(icon), name));
    {
        // 侧栏宽度随语言自适应，避免英文被截断
        QFont nf = m_nav->font();
        nf.setPixelSize(13);
        const QFontMetrics fm(nf);
        int need = 0;
        for (const auto& [icon, name] : nav) need = qMax(need, fm.horizontalAdvance(name));
        QFont bf = brand->font();
        bf.setPixelSize(15);
        bf.setWeight(QFont::DemiBold);
        const int brandNeed = QFontMetrics(bf).horizontalAdvance(brand->text()) + 24 + 8 + 8;
        side->setFixedWidth(qBound(196, qMax(need + 18 + 8 + 8 + 16, brandNeed) + 24 + 8, 260));
    }
    for (QWidget* p : {buildGeneral(), buildCapture(), buildAnnotate(), buildRecord(), buildUpload(), buildAppearance(), buildAbout()}) {
        auto* sa = new QScrollArea;
        sa->setWidgetResizable(true);
        sa->setFrameShape(QFrame::NoFrame);
        sa->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        sa->setWidget(p);
        m_stack->addWidget(sa);
    }
    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    QSettings st(QStringLiteral("QingYing"), QStringLiteral("QingYing"));
    const QSize sz = st.value("settingsSize", QSize(780, 560)).toSize();
    resize(sz.expandedTo(minimumSize()));
}

SettingsDialog::~SettingsDialog()
{
    if (m_upTimer && m_upTimer->isActive()) saveUpload(); // 关闭时写入未保存的请求头
    if (m_hotkey && m_hotkey->isRecording() && m_suspend) m_suspend(false);
    QSettings st(QStringLiteral("QingYing"), QStringLiteral("QingYing"));
    if (!isMaximized()) st.setValue("settingsSize", size());
}

int SettingsDialog::page() const { return m_nav->currentRow(); }
void SettingsDialog::setPage(int i) { m_nav->setCurrentRow(qBound(0, i, m_nav->count() - 1)); }
int SettingsDialog::pageCount() const { return m_nav->count(); }

QWidget* SettingsDialog::makePage(const QString& title, const QString& subtitle, QVBoxLayout*& body)
{
    auto* w = new QWidget;
    body = new QVBoxLayout(w);
    body->setContentsMargins(28, 24, 28, 20);
    body->setSpacing(0);
    auto* t = new QLabel(title, w);
    t->setObjectName(QStringLiteral("pageTitle"));
    body->addWidget(t);
    auto* s = new QLabel(subtitle, w);
    s->setObjectName(QStringLiteral("pageSub"));
    s->setWordWrap(true);
    body->addSpacing(4);
    body->addWidget(s);
    body->addSpacing(18);
    return w;
}

void SettingsDialog::setStatus(QLabel* l, const QString& text, bool warn)
{
    l->setText(text);
    l->setProperty("state", warn ? QStringLiteral("warn") : QString());
    l->style()->unpolish(l);
    l->style()->polish(l);
}

QWidget* SettingsDialog::buildGeneral()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "常规"), QCoreApplication::translate("QingYing", "快捷键、启动与历史记录"), b);
    Card c = addCard(b, QCoreApplication::translate("QingYing", "快捷键"));
    m_hotkey = new HotkeyEdit;
    m_hotkey->setSequence(Settings::hotkey());
    auto* reset = new QPushButton(QCoreApplication::translate("QingYing", "恢复默认"));
    addRow(c, QCoreApplication::translate("QingYing", "截图"), QCoreApplication::translate("QingYing", "点击后按下新的组合键，Esc 取消"), hbox({m_hotkey, reset}, -1), false, &m_hotkeyHint);
    auto tryApply = [this](QKeySequence k) {
        if (k.isEmpty()) k = Settings::defaultHotkey();
        unsigned m = 0, v = 0;
        if (!HotkeyManager::toNative(k, m, v) || m == 0) {
            setStatus(m_hotkeyHint, QCoreApplication::translate("QingYing", "需要包含 Ctrl / Alt / Shift / Win 之一，并使用字母、数字或功能键"), true);
            return;
        }
        if (!m_apply(k)) {
            setStatus(m_hotkeyHint, QCoreApplication::translate("QingYing", "%1 已被其他程序占用，请换一个组合").arg(k.toString(QKeySequence::NativeText)), true);
            return;
        }
        Settings::setHotkey(k);
        m_hotkey->setSequence(k);
        setStatus(m_hotkeyHint, QCoreApplication::translate("QingYing", "已生效：%1").arg(k.toString(QKeySequence::NativeText)), false);
        emit changed();
    };
    connect(m_hotkey, &HotkeyEdit::recordingChanged, this, [this](bool on) {
        if (m_suspend) m_suspend(on);
    });
    connect(m_hotkey, &HotkeyEdit::recorded, this, tryApply);
    connect(reset, &QPushButton::clicked, this, [tryApply] { tryApply(Settings::defaultHotkey()); });

    c = addCard(b, QCoreApplication::translate("QingYing", "启动"));
    addRow(c, QCoreApplication::translate("QingYing", "开机自动启动"), QCoreApplication::translate("QingYing", "登录 Windows 后在托盘中待命"), toggle(Settings::autostart(), [this](bool on) {
               Settings::setAutostart(on);
               emit changed();
           }));

    c = addCard(b, QCoreApplication::translate("QingYing", "历史记录"));
    auto* hist = new QSpinBox;
    hist->setRange(0, 500);
    hist->setValue(Settings::historySize());
    hist->setSuffix(QCoreApplication::translate("QingYing", " 张"));
    hist->setSpecialValueText(QCoreApplication::translate("QingYing", "不保留"));
    addRow(c, QCoreApplication::translate("QingYing", "保留数量"), QCoreApplication::translate("QingYing", "最近的截图可在“历史记录”中重新复制、钉图或编辑"), hist);
    connect(hist, &QSpinBox::valueChanged, this, [this](int n) {
        Settings::setHistorySize(n);
        emit changed();
    });
    b->addStretch();
    return w;
}

QWidget* SettingsDialog::buildCapture()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "截图"), QCoreApplication::translate("QingYing", "保存位置、文件名与截图完成后的动作"), b);
    Card c = addCard(b, QCoreApplication::translate("QingYing", "保存"));
    m_dir = new QLineEdit(QDir::toNativeSeparators(Settings::saveDir()));
    auto* browse = new QPushButton(QCoreApplication::translate("QingYing", "浏览…"));
    auto* open = iconButton(Icon::Folder, QCoreApplication::translate("QingYing", "打开文件夹"));
    addRow(c, QCoreApplication::translate("QingYing", "保存位置"), QCoreApplication::translate("QingYing", "点击“保存”或按 Ctrl+S 时写入这里"), hbox({m_dir, browse, open}), true, &m_dirHint);
    auto commitDir = [this] {
        const QString d = QDir::fromNativeSeparators(m_dir->text().trimmed());
        const bool ok = !d.isEmpty() && QDir::isAbsolutePath(d) && QDir().mkpath(d);
        m_dir->setProperty("state", ok ? QString() : QStringLiteral("warn"));
        m_dir->style()->unpolish(m_dir);
        m_dir->style()->polish(m_dir);
        if (!ok) {
            setStatus(m_dirHint, QCoreApplication::translate("QingYing", "无法使用该文件夹，请选择一个可写入的完整路径"), true);
            return;
        }
        setStatus(m_dirHint, QCoreApplication::translate("QingYing", "点击“保存”或按 Ctrl+S 时写入这里"), false);
        if (d != Settings::saveDir()) {
            Settings::setSaveDir(d);
            emit changed();
        }
    };
    connect(m_dir, &QLineEdit::editingFinished, this, commitDir);
    connect(browse, &QPushButton::clicked, this, [this, commitDir] {
        const QString d = QFileDialog::getExistingDirectory(this, QCoreApplication::translate("QingYing", "选择保存位置"), m_dir->text());
        if (d.isEmpty()) return;
        m_dir->setText(QDir::toNativeSeparators(d));
        commitDir();
    });
    connect(open, &QToolButton::clicked, this, [this] {
        const QString d = QDir::fromNativeSeparators(m_dir->text().trimmed());
        QDir().mkpath(d);
        QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    });

    m_pattern = new QLineEdit(Settings::filePattern());
    m_pattern->setPlaceholderText(QStringLiteral("QingYing_{yyyyMMdd_HHmmss}"));
    m_preview = new QLabel;
    m_preview->setObjectName(QStringLiteral("preview"));
    m_preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* pw = new QWidget;
    auto* pl = new QVBoxLayout(pw);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(6);
    pl->addWidget(m_pattern);
    pl->addWidget(m_preview);
    addRow(c, QCoreApplication::translate("QingYing", "文件名"), QCoreApplication::translate("QingYing", "{} 内为日期时间格式，如 {yyyyMMdd_HHmmss}；留空使用默认"), pw, true);
    connect(m_pattern, &QLineEdit::textChanged, this, [this](const QString& t) {
        updatePreview();
        const QString v = t.trimmed().isEmpty() ? QStringLiteral("QingYing_{yyyyMMdd_HHmmss}") : t.trimmed();
        if (v != Settings::filePattern()) {
            Settings::setFilePattern(v);
            emit changed();
        }
    });

    auto* fmt = new Segmented({QStringLiteral("PNG"), QStringLiteral("JPG")});
    fmt->setCurrent(Settings::format() == QLatin1String("jpg") ? 1 : 0);
    addRow(c, QCoreApplication::translate("QingYing", "格式"), QCoreApplication::translate("QingYing", "PNG 无损清晰；JPG 体积更小"), fmt);
    connect(fmt, &Segmented::changed, this, [this](int i) {
        Settings::setFormat(i == 1 ? QStringLiteral("jpg") : QStringLiteral("png"));
        updatePreview();
        emit changed();
    });

    c = addCard(b, QCoreApplication::translate("QingYing", "截图完成"));
    auto* act = new Segmented({QCoreApplication::translate("QingYing", "复制"), QCoreApplication::translate("QingYing", "保存"), QCoreApplication::translate("QingYing", "钉图")});
    act->setCurrent(qMax(0, int(kActions.indexOf(Settings::defaultAction()))));
    addRow(c, QCoreApplication::translate("QingYing", "默认动作"), QCoreApplication::translate("QingYing", "按 Enter、双击选区或点击 ✓ 时执行"), act);
    connect(act, &Segmented::changed, this, [this](int i) {
        Settings::setDefaultAction(kActions.value(i, kActions[0]));
        emit changed();
    });
    auto* cf = new QComboBox;
    cf->addItem(QCoreApplication::translate("QingYing", "#4F6BFF（HEX 大写）"), QStringLiteral("hex"));
    cf->addItem(QCoreApplication::translate("QingYing", "#4f6bff（HEX 小写）"), QStringLiteral("hexlower"));
    cf->addItem(QStringLiteral("rgb(79, 107, 255)"), QStringLiteral("rgb"));
    cf->addItem(QStringLiteral("79, 107, 255"), QStringLiteral("rgbplain"));
    cf->setCurrentIndex(qMax(0, cf->findData(Settings::colorFormat())));
    addRow(c, QCoreApplication::translate("QingYing", "取色格式"), QCoreApplication::translate("QingYing", "取色器按 C 复制颜色时使用"), cf);
    connect(cf, &QComboBox::currentIndexChanged, this, [this, cf] {
        Settings::setColorFormat(cf->currentData().toString());
        emit changed();
    });

    c = addCard(b, QCoreApplication::translate("QingYing", "识别与翻译"));
    addRow(c, QCoreApplication::translate("QingYing", "联网翻译 OCR 结果"),
           QCoreApplication::translate("QingYing", "识别面板提供译成中文 / 英文；使用 MyMemory 公开接口，无需密钥，可随时关闭"),
           toggle(Settings::translateEnabled(), [this](bool on) {
               Settings::setTranslateEnabled(on);
               emit changed();
           }));

    b->addStretch();
    updatePreview();
    return w;
}

QWidget* SettingsDialog::buildAnnotate()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "标注"), QCoreApplication::translate("QingYing", "进入截图编辑时预先选中的工具与颜色"), b);
    Card c = addCard(b, QCoreApplication::translate("QingYing", "默认值"));
    auto* tool = new QComboBox;
    const std::pair<Icon, QString> tools[] = {
        {Icon::Capture, QCoreApplication::translate("QingYing", "无")},   {Icon::Rect, QCoreApplication::translate("QingYing", "矩形")}, {Icon::Ellipse, QCoreApplication::translate("QingYing", "椭圆")}, {Icon::Arrow, QCoreApplication::translate("QingYing", "箭头")},
        {Icon::Pen, QCoreApplication::translate("QingYing", "画笔")},     {Icon::Text, QCoreApplication::translate("QingYing", "文字")}, {Icon::Mosaic, QCoreApplication::translate("QingYing", "马赛克")}, {Icon::Number, QCoreApplication::translate("QingYing", "序号")},
    };
    for (const auto& [icon, name] : tools) tool->addItem(menuIcon(icon), name);
    tool->setCurrentIndex(Settings::defaultTool());
    addRow(c, QCoreApplication::translate("QingYing", "默认工具"), QCoreApplication::translate("QingYing", "选择“无”时先调整选区，再手动选工具"), tool);
    connect(tool, &QComboBox::currentIndexChanged, this, [this](int i) {
        Settings::setDefaultTool(i);
        emit changed();
    });
    auto* color = new QComboBox;
    const QList<QColor> palette = Style::annotationColors();
    const qreal dpr = devicePixelRatioF();
    for (const QColor& col : palette) {
        QPixmap sw(QSize(14, 14) * dpr);
        sw.setDevicePixelRatio(dpr);
        sw.fill(Qt::transparent);
        QPainter sp(&sw);
        sp.setRenderHint(QPainter::Antialiasing);
        sp.setPen(QColor(0, 0, 0, 60));
        sp.setBrush(col);
        sp.drawEllipse(QRectF(1, 1, 12, 12));
        sp.end();
        color->addItem(QIcon(sw), col.name().toUpper());
    }
    color->setCurrentIndex(qBound(0, Settings::defaultColor(), int(palette.size()) - 1));
    addRow(c, QCoreApplication::translate("QingYing", "默认颜色"), QCoreApplication::translate("QingYing", "矩形、箭头、画笔等标注的初始颜色"), color);
    connect(color, &QComboBox::currentIndexChanged, this, [this](int i) {
        Settings::setDefaultColor(i);
        emit changed();
    });
    b->addStretch();
    return w;
}

QWidget* SettingsDialog::buildRecord()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "录屏"), QCoreApplication::translate("QingYing", "录制 MP4 / GIF；麦克风仅用于 MP4"), b);
    Card c = addCard(b, QCoreApplication::translate("QingYing", "画面"));
    auto* fps = new Segmented({QStringLiteral("10 fps"), QStringLiteral("15 fps"), QStringLiteral("30 fps")});
    const int cur = Settings::recordFps();
    fps->setCurrent(cur <= 10 ? 0 : cur >= 30 ? 2 : 1);
    addRow(c, QCoreApplication::translate("QingYing", "帧率"), QCoreApplication::translate("QingYing", "帧率越高越流畅，文件也越大"), fps);
    connect(fps, &Segmented::changed, this, [this](int i) {
        Settings::setRecordFps(kFps[qBound(0, i, 2)]);
        emit changed();
    });
    addRow(c, QCoreApplication::translate("QingYing", "录制鼠标指针"), QCoreApplication::translate("QingYing", "在画面中绘制鼠标指针"), toggle(Settings::recordCursor(), [this](bool on) {
               Settings::setRecordCursor(on);
               emit changed();
           }));
    addRow(c, QCoreApplication::translate("QingYing", "录制麦克风"), QCoreApplication::translate("QingYing", "仅 MP4；默认关闭。GIF 不支持录音"), toggle(Settings::recordMic(), [this](bool on) {
               Settings::setRecordMic(on);
               emit changed();
           }));
    c = addCard(b, QStringLiteral("GIF"));
    addRow(c, QCoreApplication::translate("QingYing", "抖动"), QCoreApplication::translate("QingYing", "减少渐变色带，文件稍大"), toggle(Settings::gifDither(), [this](bool on) {
               Settings::setGifDither(on);
               emit changed();
           }));
    b->addStretch();
    return w;
}

QWidget* SettingsDialog::buildUpload()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "上传"), QCoreApplication::translate("QingYing", "上传截图到图床，并把链接复制到剪贴板"), b);
    const Uploads::Config up = Settings::uploadConfig();
    Card c = addCard(b, QString());
    addRow(c, QCoreApplication::translate("QingYing", "启用上传"), QCoreApplication::translate("QingYing", "截图工具条与钉图菜单中出现“上传”"), toggle(Settings::uploadEnabled(), [this](bool on) {
               Settings::setUploadEnabled(on);
               m_upBody->setEnabled(on);
               emit changed();
           }));

    m_upBody = new QWidget;
    auto* ub = new QVBoxLayout(m_upBody);
    ub->setContentsMargins(0, 0, 0, 0);
    ub->setSpacing(0);
    b->addWidget(m_upBody);
    Card s = addCard(ub, QCoreApplication::translate("QingYing", "服务"));
    m_upProvider = new QComboBox;
    m_upProvider->addItem(QCoreApplication::translate("QingYing", "自定义 HTTP（multipart POST）"), QStringLiteral("custom"));
    m_upProvider->addItem(QStringLiteral("SM.MS"), QStringLiteral("smms"));
    m_upProvider->addItem(QStringLiteral("Imgur"), QStringLiteral("imgur"));
    m_upProvider->setCurrentIndex(qMax(0, m_upProvider->findData(up.provider)));
    addRow(s, QCoreApplication::translate("QingYing", "服务"), QCoreApplication::translate("QingYing", "选择图床；下方只显示该服务需要的参数"), m_upProvider);

    m_upCustom = new QWidget;
    auto* cl = new QVBoxLayout(m_upCustom);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    Card cc = addCard(cl, QCoreApplication::translate("QingYing", "自定义 HTTP"));
    m_upUrl = new QLineEdit(up.url);
    m_upUrl->setPlaceholderText(QStringLiteral("https://example.com/upload"));
    addRow(cc, QCoreApplication::translate("QingYing", "上传地址"), QCoreApplication::translate("QingYing", "接收 multipart/form-data POST 的地址"), m_upUrl, true, &m_upHint);
    m_upField = new QLineEdit(up.field);
    m_upField->setPlaceholderText(QStringLiteral("file"));
    m_upField->setMaximumWidth(220);
    addRow(cc, QCoreApplication::translate("QingYing", "文件字段"), QCoreApplication::translate("QingYing", "表单中图片文件的字段名"), m_upField);
    m_upPath = new QLineEdit(up.jsonPath);
    m_upPath->setPlaceholderText(QStringLiteral("data.url"));
    m_upPath->setMaximumWidth(220);
    addRow(cc, QCoreApplication::translate("QingYing", "链接路径"), QCoreApplication::translate("QingYing", "返回 JSON 中链接的位置，如 data.url"), m_upPath);
    m_upHeaders = new QPlainTextEdit;
    m_upHeaders->setPlaceholderText(QCoreApplication::translate("QingYing", "每行一个，如 Authorization: Bearer xxx"));
    m_upHeaders->setFixedHeight(72);
    addRow(cc, QCoreApplication::translate("QingYing", "请求头"), QCoreApplication::translate("QingYing", "可选；以 DPAPI 加密保存"), m_upHeaders, true);
    ub->addWidget(m_upCustom);

    m_upPreset = new QWidget;
    auto* pl = new QVBoxLayout(m_upPreset);
    pl->setContentsMargins(0, 0, 0, 0);
    pl->setSpacing(0);
    Card pc = addCard(pl, QCoreApplication::translate("QingYing", "凭据"));
    m_upToken = new QLineEdit;
    m_upToken->setEchoMode(QLineEdit::Password);
    addRow(pc, QStringLiteral("Token"), QCoreApplication::translate("QingYing", "使用 Windows DPAPI 加密保存，仅当前用户可解密"), m_upToken, true);
    ub->addWidget(m_upPreset);

    if (up.provider == QLatin1String("custom")) m_upHeaders->setPlainText(up.secret);
    else m_upToken->setText(up.secret);
    connect(m_upProvider, &QComboBox::currentIndexChanged, this, [this] {
        updateUploadUi();
        saveUpload();
    });
    for (QLineEdit* e : {m_upUrl, m_upField, m_upPath, m_upToken}) connect(e, &QLineEdit::editingFinished, this, &SettingsDialog::saveUpload);
    m_upTimer = new QTimer(this);
    m_upTimer->setSingleShot(true);
    m_upTimer->setInterval(500);
    connect(m_upTimer, &QTimer::timeout, this, &SettingsDialog::saveUpload);
    connect(m_upHeaders, &QPlainTextEdit::textChanged, m_upTimer, qOverload<>(&QTimer::start));
    m_upBody->setEnabled(Settings::uploadEnabled());
    updateUploadUi();
    b->addStretch();
    return w;
}

void SettingsDialog::updateUploadUi()
{
    const QString p = m_upProvider->currentData().toString();
    const bool custom = p == QLatin1String("custom");
    m_upCustom->setVisible(custom);
    m_upPreset->setVisible(!custom);
    m_upToken->setPlaceholderText(Uploads::create({p})->secretHint());
}

void SettingsDialog::saveUpload()
{
    if (m_upTimer) m_upTimer->stop();
    Uploads::Config up;
    up.provider = m_upProvider->currentData().toString();
    up.url = m_upUrl->text().trimmed();
    up.field = m_upField->text().trimmed();
    up.jsonPath = m_upPath->text().trimmed();
    up.secret = up.provider == QLatin1String("custom") ? m_upHeaders->toPlainText() : m_upToken->text();
    if (up.provider == QLatin1String("custom")) {
        const QUrl u(up.url, QUrl::StrictMode);
        const bool ok = up.url.isEmpty() || (u.isValid() && (u.scheme() == QLatin1String("https") || u.scheme() == QLatin1String("http")) && !u.host().isEmpty());
        setStatus(m_upHint, ok ? QCoreApplication::translate("QingYing", "接收 multipart/form-data POST 的地址") : QCoreApplication::translate("QingYing", "请输入以 http:// 或 https:// 开头的完整地址"), !ok);
        m_upUrl->setProperty("state", ok ? QString() : QStringLiteral("warn"));
        m_upUrl->style()->unpolish(m_upUrl);
        m_upUrl->style()->polish(m_upUrl);
        if (!ok) return;
    }
    Settings::setUploadConfig(up);
    emit changed();
}

QWidget* SettingsDialog::buildAppearance()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "外观与语言"), QCoreApplication::translate("QingYing", "修改后立即生效，无需重启"), b);
    Card c = addCard(b, QCoreApplication::translate("QingYing", "外观"));
    auto* theme = new Segmented({QCoreApplication::translate("QingYing", "跟随系统"), QCoreApplication::translate("QingYing", "浅色"), QCoreApplication::translate("QingYing", "深色")});
    theme->setCurrent(qMax(0, int(kThemes.indexOf(Settings::theme()))));
    addRow(c, QCoreApplication::translate("QingYing", "主题"), QCoreApplication::translate("QingYing", "跟随系统时随 Windows 深浅色自动切换"), theme);
    connect(theme, &Segmented::changed, this, [this](int i) {
        Settings::setTheme(kThemes.value(i, kThemes[0]));
        Theme::apply();
        emit changed();
    });
    c = addCard(b, QCoreApplication::translate("QingYing", "语言"));
    auto* lang = new QComboBox;
    lang->addItem(QCoreApplication::translate("QingYing", "跟随系统"), QStringLiteral("system"));
    lang->addItem(QString::fromUtf8("\u7b80\u4f53\u4e2d\u6587"), QStringLiteral("zh_CN"));
    lang->addItem(QStringLiteral("English"), QStringLiteral("en_US"));
    lang->setCurrentIndex(qMax(0, lang->findData(Settings::language())));
    addRow(c, QCoreApplication::translate("QingYing", "界面语言"), QCoreApplication::translate("QingYing", "Interface language"), lang);
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] {
        const QString before = Settings::effectiveLanguage();
        Settings::setLanguage(lang->currentData().toString());
        emit changed();
        if (Settings::effectiveLanguage() != before) {
            Language::apply(Settings::effectiveLanguage());
            emit languageChanged();
        }
    });
    b->addStretch();
    return w;
}

QWidget* SettingsDialog::buildAbout()
{
    QVBoxLayout* b;
    QWidget* w = makePage(QCoreApplication::translate("QingYing", "关于与更新"), QCoreApplication::translate("QingYing", "版本信息、更新与诊断"), b);
    Card c = addCard(b, QString());
    auto* head = new QWidget;
    auto* hl = new QHBoxLayout(head);
    hl->setContentsMargins(16, 16, 16, 16);
    hl->setSpacing(14);
    auto* logo = new QLabel;
    logo->setPixmap(appIcon().pixmap(56, 56));
    hl->addWidget(logo);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(2);
    auto* name = new QLabel(QCoreApplication::translate("QingYing", "青影 QingYing"));
    name->setObjectName(QStringLiteral("aboutName"));
    auto* ver = new QLabel(QCoreApplication::translate("QingYing", "版本 %1 · 简洁好看的 Windows 截图工具").arg(QApplication::applicationVersion()));
    ver->setObjectName(QStringLiteral("rowHint"));
    ver->setWordWrap(true);
    tv->addWidget(name);
    tv->addWidget(ver);
    hl->addLayout(tv, 1);
    c.lay->addWidget(head);
    c.rows = 1;

    auto* check = new QPushButton(QCoreApplication::translate("QingYing", "检查更新"));
    check->setObjectName(QStringLiteral("primary"));
    const QDateTime last = Settings::lastUpdateCheck();
    addRow(c, QCoreApplication::translate("QingYing", "更新"), last.isValid() ? QCoreApplication::translate("QingYing", "上次检查：%1").arg(last.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))) : QCoreApplication::translate("QingYing", "尚未检查"), check);
    connect(check, &QPushButton::clicked, this, [this] {
        if (m_checkUpdate) m_checkUpdate();
    });
    addRow(c, QCoreApplication::translate("QingYing", "自动检查更新"), QCoreApplication::translate("QingYing", "启动时检查，每天最多一次"), toggle(Settings::autoUpdate(), [this](bool on) {
               Settings::setAutoUpdate(on);
               emit changed();
           }));

    c = addCard(b, QCoreApplication::translate("QingYing", "更多"));
    auto* gh = new QPushButton(QCoreApplication::translate("QingYing", "打开 GitHub"));
    addRow(c, QCoreApplication::translate("QingYing", "项目主页"), QString::fromLatin1(kRepoUrl).mid(8), gh);
    connect(gh, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QString::fromLatin1(kRepoUrl))); });
    auto* logs = new QPushButton(QCoreApplication::translate("QingYing", "打开日志文件夹"));
    addRow(c, QCoreApplication::translate("QingYing", "日志"), QCoreApplication::translate("QingYing", "反馈问题时可附上日志与崩溃转储"), logs);
    connect(logs, &QPushButton::clicked, this, [] {
        QDir().mkpath(Diagnostics::logDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(Diagnostics::logDir()));
    });
    b->addStretch();
    return w;
}

void SettingsDialog::updatePreview()
{
    const QString ext = Settings::format() == QLatin1String("jpg") ? QStringLiteral(".jpg") : QStringLiteral(".png");
    const QString pat = m_pattern->text().trimmed().isEmpty() ? QStringLiteral("QingYing_{yyyyMMdd_HHmmss}") : m_pattern->text();
    m_preview->setText(QCoreApplication::translate("QingYing", "预览：%1").arg(Settings::expandPattern(pat) + ext));
}
