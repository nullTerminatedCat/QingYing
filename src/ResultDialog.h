#pragma once
#include "AskText.h"

#include <QDialog>
#include <QList>

class QEvent;
class QKeyEvent;
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
    // 配好 AI 回到面板后要接上的动作。Continue 只收起提示。
    enum class SetupNext { Continue, Ask, Rewrite };
    // 只亮出「打开设置」，不改当前状态（识别进行中就能点）。
    void showAiSettingsButton(SetupNext next);
    // 用一句说明替换状态行，并聚焦「打开设置」。status 为空时用默认的 AI 未配置说明。
    void showAiSetupNeeded(SetupNext next, const QString& status = {});

protected:
    void changeEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void startTranslate(bool toChinese);
    void setBusy(bool busy);
    void retranslateUi();
    void flashCopyDone();
    void syncAssistButtons();
    void noteSettingsReturned();
    bool handleAssistKey(QKeyEvent* key);
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
    QWidget* m_rewriteRow = nullptr;
    QList<QPushButton*> m_rewriteChips;
    QPushButton* m_openAi = nullptr;
    QTimer* m_copyFlash = nullptr;
    QString m_url;
    QString m_original;
    bool m_busy = false;
    bool m_wantTranslate = false;
    bool m_wantAssist = false;
    SetupNext m_setupNext = SetupNext::Continue;
    int m_translateGen = 0;
};