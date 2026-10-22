#pragma once
#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;

// OCR / 二维码结果面板：可编辑文本、复制、翻译、（可选）打开链接
class ResultDialog : public QDialog {
    Q_OBJECT
public:
    ResultDialog(const QString& title, QWidget* parent = nullptr);
    void setStatus(const QString& status, bool error = false);
    void setText(const QString& text);
    void setUrl(const QString& url);
    // OCR 面板打开翻译按钮；二维码等默认关闭
    void setTranslateEnabled(bool on);

private:
    void startTranslate(bool toChinese);
    void setBusy(bool busy);

    QLabel* m_status = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_toZh = nullptr;
    QPushButton* m_toEn = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_auto = nullptr;
    QString m_url;
    QString m_original;
    bool m_busy = false;
};