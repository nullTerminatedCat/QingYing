#pragma once
#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit HotkeyManager(QObject* parent = nullptr);
    ~HotkeyManager() override;

    // 注册全局热键；失败（被占用或不支持的按键）返回 false
    bool setHotkey(const QKeySequence& seq);
    void unregister();
    QKeySequence current() const { return m_seq; }
    bool isRegistered() const { return m_registered; }
    static bool toNative(const QKeySequence& seq, unsigned& mods, unsigned& vk);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void activated();

private:
    static constexpr int kId = 0x5159;
    bool m_registered = false;
    QKeySequence m_seq;
};
