#pragma once
#include <QAbstractButton>
#include <QFrame>
#include <QKeySequence>
#include <QPushButton>

class QButtonGroup;
class QVariantAnimation;

// 开关：替代复选框，用于布尔设置
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVariantAnimation* m_anim;
    double m_pos = 0; // 0 关 … 1 开
};

// 分段控件：少量互斥选项
class Segmented : public QFrame {
    Q_OBJECT
public:
    explicit Segmented(const QStringList& labels, QWidget* parent = nullptr);
    int current() const;
    void setCurrent(int i);
signals:
    void changed(int index);

private:
    QButtonGroup* m_group;
};

// 快捷键录制框：点击后按下组合键；Esc 取消，Backspace 清除
class HotkeyEdit : public QPushButton {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget* parent = nullptr);
    void setSequence(const QKeySequence& k);
    QKeySequence sequence() const { return m_seq; }
    bool isRecording() const { return m_recording; }
signals:
    void recorded(const QKeySequence& k); // 空序列表示清除
    void recordingChanged(bool on);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void setRecording(bool on);
    void refresh(Qt::KeyboardModifiers partial = {});
    QKeySequence m_seq;
    bool m_recording = false;
};
