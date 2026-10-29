#pragma once
#include "AskText.h"

#include <QDialog>

class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QWidget;

// OCR / 二维码结果面板：可编辑文本、复制、翻译、（可选）打开链接
class ResultDialog : public QDialog {
    Q_OBJECT
public:
    ResultDialog(const QString& title, QWidget* parent = nullptr);
    ~ResultDialog() override;
    void setStatus(const QString& status, bool error = false);
    void setText(const QString& text);
    void setUrl(const QString& url);
    // OCR 面板打开翻译按钮；二维码等默认关闭
    void setTranslateEnabled(bool on);
    // 识别进行中：编辑框只读、禁用操作，避免空结果误点
    void setWorking(bool working);
    // 按文本语言自动翻译。未开启联网翻译时只提示，不请求。已是目标语言等跳过逻辑与手动翻译相同。
    void translateSuggested();
    // OCR 面板上的提问与改文案。二维码等默认关闭。
    void setAssistEnabled(bool on);
    void focusQuestion();
    // 按当前正文改写，或在提问框有内容时提问。没有文字 / 问题为空时不请求。
    void startAssist(AskText::Mode mode);

protected:
    void changeEvent(QEvent* event) override;

private:
    void startTranslate(bool toChinese);
    void setBusy(bool busy);
    void retranslateUi();
    void flashCopyDone();
    void syncAssistButtons();
    QString sourceForAssist() const;

    QString m_panelTitle;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_status = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_close = nullptr;
    QPushButton* m_open = nullptr;
    QPushButton* m_toZh = nullptr;
    QPushButton* m_toEn = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_auto = nullptr;
    QWidget* m_askRow = nullptr;
    QLineEdit* m_question = nullptr;
    QPushButton* m_ask = nullptr;
    QPushButton* m_rewrite = nullptr;
    QTimer* m_copyFlash = nullptr;
    QString m_url;
    QString m_original;
    bool m_busy = false;
    bool m_wantTranslate = false;
    bool m_wantAssist = false;
    int m_translateGen = 0;
};