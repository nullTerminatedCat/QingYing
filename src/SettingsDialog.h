#pragma once
#include <QDialog>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class QComboBox;
class QPlainTextEdit;
class HotkeyEdit;

// 设置：左侧导航 + 分组卡片，修改即时生效
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    // applyCapture / applyRecord：尝试注册新热键，失败（冲突）返回 false
    explicit SettingsDialog(std::function<bool(const QKeySequence&)> applyCapture,
                            std::function<bool(const QKeySequence&)> applyRecord,
                            QWidget* parent = nullptr);
    ~SettingsDialog() override;
    void setCheckUpdate(std::function<void()> f) { m_checkUpdate = std::move(f); }
    // 录制快捷键期间暂停全局热键（true 暂停，false 恢复），以便录到当前组合
    void setSuspendHotkey(std::function<void(bool)> f) { m_suspend = std::move(f); }
    int page() const;
    void setPage(int i);
    int pageCount() const;
signals:
    void languageChanged(); // 界面语言已切换，调用方负责重建窗口
    void changed();         // 任一设置被修改

private:
    QWidget* buildGeneral();
    QWidget* buildCapture();
    QWidget* buildAnnotate();
    QWidget* buildRecord();
    QWidget* buildUpload();
    QWidget* buildAppearance();
    QWidget* buildAbout();
    QWidget* makePage(const QString& title, const QString& subtitle, QVBoxLayout*& body);
    void updatePreview();
    void updateUploadUi();
    void saveUpload();
    void setStatus(QLabel* l, const QString& text, bool warn);

    std::function<bool(const QKeySequence&)> m_applyCapture;
    std::function<bool(const QKeySequence&)> m_applyRecord;
    std::function<void()> m_checkUpdate;
    std::function<void(bool)> m_suspend;
    class QTimer* m_upTimer = nullptr;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_stack = nullptr;
    HotkeyEdit* m_hotkey = nullptr;
    QLabel* m_hotkeyHint = nullptr;
    HotkeyEdit* m_recordHotkey = nullptr;
    QLabel* m_recordHotkeyHint = nullptr;
    QLineEdit* m_dir = nullptr;
    QLabel* m_dirHint = nullptr;
    QLineEdit* m_pattern = nullptr;
    QLabel* m_preview = nullptr;
    QComboBox* m_upProvider = nullptr;
    QLineEdit* m_upUrl = nullptr;
    QLineEdit* m_upField = nullptr;
    QLineEdit* m_upPath = nullptr;
    QLineEdit* m_upToken = nullptr;
    QPlainTextEdit* m_upHeaders = nullptr;
    QWidget* m_upCustom = nullptr;
    QWidget* m_upPreset = nullptr;
    QWidget* m_upBody = nullptr;
    QLabel* m_upHint = nullptr;
};
