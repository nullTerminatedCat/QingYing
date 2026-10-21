#include <QCoreApplication>
#include "Theme.h"
#include "HistoryWindow.h"
#include "History.h"
#include "Icons.h"
#include "PinWindow.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include "EditRequest.h"
#include <QUrl>
#include <QVBoxLayout>

HistoryWindow::HistoryWindow(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QCoreApplication::translate("QingYing", "青影 截图历史"));
    setWindowIcon(appIcon());
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(760, 520);
    Theme::setSheet(this, QStringLiteral(R"(
        QDialog { background: #F6F7FB; }
        QLabel#title { font-size: 20px; font-weight: 600; color: #1C1C1E; }
        QListWidget { background: #FFFFFF; border: 1px solid rgba(0,0,0,15); border-radius: 10px; padding: 8px; }
        QListWidget::item { border-radius: 8px; padding: 6px; color: #6E6E73; }
        QListWidget::item:selected { background: rgba(79,107,255,31); color: #1C1C1E; }
        QPushButton { background: #FFFFFF; border: 1px solid rgba(0,0,0,30); border-radius: 10px; padding: 6px 16px; }
        QPushButton:hover { background: #F0F2FA; }
        QPushButton#primary { background: #4F6BFF; border: none; color: white; font-weight: 600; }
    )"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);
    auto* title = new QLabel(QCoreApplication::translate("QingYing", "截图历史"), this);
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);
    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(200, 130));
    m_list->setGridSize(QSize(220, 170));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(4);
    root->addWidget(m_list, 1);
    m_empty = new QLabel(QCoreApplication::translate("QingYing", "还没有截图记录\n按截图快捷键或单击托盘图标截一张试试"), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setObjectName(QStringLiteral("empty"));
    m_empty->setStyleSheet(QStringLiteral("color: #8E8E93; font-size: 14px;"));
    root->addWidget(m_empty, 1);
    auto* row = new QHBoxLayout;
    auto* folder = new QPushButton(QCoreApplication::translate("QingYing", "打开文件夹"), this);
    auto* clear = new QPushButton(QCoreApplication::translate("QingYing", "清空"), this);
    auto* edit = new QPushButton(QCoreApplication::translate("QingYing", "编辑标注"), this);
    auto* copy = new QPushButton(QCoreApplication::translate("QingYing", "复制"), this);
    auto* pin = new QPushButton(QCoreApplication::translate("QingYing", "钉在桌面"), this);
    pin->setObjectName(QStringLiteral("primary"));
    row->addWidget(folder);
    row->addWidget(clear);
    row->addStretch();
    row->addWidget(edit);
    row->addWidget(copy);
    row->addWidget(pin);
    root->addLayout(row);
    m_needItem = {clear, edit, copy, pin};

    connect(folder, &QPushButton::clicked, this, [] {
        QDir().mkpath(History::dir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(History::dir()));
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QCoreApplication::translate("QingYing", "清空历史"), QCoreApplication::translate("QingYing", "确定删除全部截图历史？")) == QMessageBox::Yes) {
            History::clear();
            reload();
        }
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        const QString p = currentPath();
        if (!p.isEmpty()) QGuiApplication::clipboard()->setImage(QImage(p));
    });
    connect(edit, &QPushButton::clicked, this, [this] {
        const QString p = currentPath();
        if (p.isEmpty()) return;
        hide(); // 不让历史窗口挡住编辑
        EditRequest::request(QImage(p), QPoint(), false, nullptr);
    });
    auto doPin = [this] {
        const QString p = currentPath();
        if (p.isEmpty()) return;
        auto* w = new PinWindow(QImage(p));
        w->move(QCursor::pos() - QPoint(40, 40));
        w->show();
    };
    connect(pin, &QPushButton::clicked, this, doPin);
    connect(m_list, &QListWidget::itemDoubleClicked, this, doPin);
    reload();
}

QString HistoryWindow::currentPath() const
{
    auto* it = m_list->currentItem();
    return it ? it->data(Qt::UserRole).toString() : QString();
}

void HistoryWindow::reload()
{
    m_list->clear();
    for (const QString& path : History::entries()) {
        QImageReader r(path);
        const QSize full = r.size();
        r.setScaledSize(full.scaled(QSize(200, 130), Qt::KeepAspectRatio));
        const QImage thumb = r.read();
        const QString stamp = QFileInfo(path).baseName().left(15);
        const QString label = QStringLiteral("%1-%2 %3:%4  %5×%6")
                                  .arg(stamp.mid(4, 2), stamp.mid(6, 2), stamp.mid(9, 2), stamp.mid(11, 2))
                                  .arg(full.width()).arg(full.height());
        auto* it = new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)), label, m_list);
        it->setData(Qt::UserRole, path);
        it->setToolTip(QDir::toNativeSeparators(path));
    }
    if (m_list->count()) m_list->setCurrentRow(0);
    const bool any = m_list->count() > 0;
    m_list->setVisible(any);
    m_empty->setVisible(!any);
    for (QPushButton* b : m_needItem) b->setEnabled(any);
}
