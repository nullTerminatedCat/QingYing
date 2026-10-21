#pragma once
#include <QDialog>
#include <functional>

class QCheckBox;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QButtonGroup;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    // applyHotkey：尝试注册新热键，失败（冲突）返回 false
    explicit SettingsDialog(std::function<bool(const QKeySequence&)> applyHotkey, QWidget* parent = nullptr);

private:
    void save();
    void updatePreview();
    std::function<bool(const QKeySequence&)> m_apply;
    QKeySequenceEdit* m_hotkey;
    QLabel* m_hotkeyHint;
    QLineEdit* m_dir;
    QLineEdit* m_pattern;
    QLabel* m_preview;
    QPushButton* m_png;
    QPushButton* m_jpg;
    QCheckBox* m_autostart;
    QSpinBox* m_history;
    QButtonGroup* m_action;
    class QComboBox* m_colorFmt;
    QButtonGroup* m_fps;
    QCheckBox* m_recCursor;
    QButtonGroup* m_theme;
    QCheckBox* m_autoUpdate;
    QCheckBox* m_upEnabled;
    class QComboBox* m_upProvider;
    QLineEdit* m_upUrl;
    QLineEdit* m_upField;
    QLineEdit* m_upPath;
    QLineEdit* m_upToken;
    class QPlainTextEdit* m_upHeaders;
    QWidget* m_upCustom;
    QWidget* m_upPreset;
    void updateUploadUi();
};
