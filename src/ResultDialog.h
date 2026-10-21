#pragma once
#include <QDialog>

class QLabel;
class QPlainTextEdit;
class QPushButton;

// OCR / 二维码结果面板：可编辑文本、复制、（可选）打开链接
class ResultDialog : public QDialog {
    Q_OBJECT
public:
    ResultDialog(const QString& title, QWidget* parent = nullptr);
    void setStatus(const QString& status, bool error = false);
    void setText(const QString& text);
    void setUrl(const QString& url);

private:
    QLabel* m_status;
    QPlainTextEdit* m_edit;
    QPushButton* m_copy;
    QPushButton* m_open;
    QString m_url;
};
