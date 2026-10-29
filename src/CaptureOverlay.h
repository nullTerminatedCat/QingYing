#pragma once
#include "Annotation.h"

#include <QImage>
#include <QList>
#include <QPointer>
#include <QWidget>

class QPlainTextEdit;
class QWheelEvent;
class QScreen;
class StyleBar;
class ToolBar;
class WindowDetector;

enum class CaptureMode { Normal, LongShot, Pick, Record };

// 单个显示器上的冻结截图遮罩（每屏一个，保证混合 DPI 下像素精确）
class CaptureOverlay : public QWidget {
    Q_OBJECT
public:
    CaptureOverlay(QScreen* screen, const QImage& shot, const QRect& monitorPhysical, const WindowDetector* detector);
    void setPassive(bool passive);
    void activate();
    // 键盘跟随光标：鼠标进入本屏时轻量抢焦点（不 SetForegroundWindow）
    void claimKeyboard();
    void setControlsMode(bool controls);
    bool controlsMode() const { return m_controls; }

signals:
    void selectionStarted(CaptureOverlay* self);
    void selectionCleared(CaptureOverlay* self);
    void finished();
    // 请求对选区（全局物理像素）进行滚动长截图
    void longShotRequested(const QRect& physicalGlobal);
    void recordRequested(const QRect& physicalGlobal);
    void colorPicked(const QString& text);
    // Tab 切换「窗口 / 窗口+控件」时广播，多屏同步
    void controlsModeChanged(bool controls);
    // 截图已产出（复制/保存/钉图），savedPath 非空表示已写入文件
    void delivered(const QImage& image, const QString& savedPath);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    bool eventFilter(QObject* o, QEvent* e) override;

private:
    enum class State { Idle, Selecting, Selected };
    enum Handle { None, Move, TL, T, TR, R, BR, B, BL, L };

    QPoint toPhysical(const QPointF& logical) const;
    QRect toLogical(const QRect& physical) const;
    QRect physicalSelection() const;
    Handle handleAt(const QPoint& p) const;
    QList<QPointF> handlePoints() const;
    void updateHover(const QPoint& pos);
    void updateCursor(const QPoint& pos);
    void placeBars();
    void setTool(Tool t);
    void applyStyleToBar();
    qreal currentWidth() const;
    int currentFontSize() const;
    void resetSelection();
    void pushAnnotation(const Annotation& a);
    void undo();
    void redo();
    void refreshUndo();
    void beginText(const QPoint& pos);
    void commitText();
    void cancelText();
    QImage renderResult() const;
    void doCopy();
    void doSave(bool saveAs);
    void resizeEditor();
    QString editorStyle() const;
    void doPin();
    void doDefault();
    void doLongShot();
    void doRecognize(bool qr, bool autoTranslate = false);
    void doAsk();
    void popupRewrite();
    void doUpload();
public:
    void setMode(CaptureMode m) { m_mode = m; }
    // 当前屏有选区时识别并自动翻译，然后关闭遮罩。没有选区返回 false。
    bool recognizeAndTranslate();
    // 当前屏有选区时识别并打开提问。没有选区返回 false。
    bool askSelection();
    // 编辑模式：预先选中 localPhysical（相对本显示器的物理像素），pinResult=true 时完成动作改为钉图
    void presetSelection(const QRect& localPhysical, bool pinResult);
signals:
    void pinned();
private:
    bool m_pinResult = false;
    void applyDefaultTool();
private:
    CaptureMode m_mode = CaptureMode::Normal;
    void copyHex();
    void cycleColorFormat(bool reverse = false);
    void paintMagnifier(QPainter& p);
    bool magnifierVisible() const;

    QScreen* m_screen;
    QImage m_raw;     // 物理像素，dpr = 1
    QImage m_shot;    // 同一张图，带 devicePixelRatio
    QImage m_mosaic;  // 低分辨率版本，放大即马赛克
    QRect m_monitor;  // 物理像素
    qreal m_dpr = 1;
    QSizeF m_canvas;
    const WindowDetector* m_detector;

    State m_state = State::Idle;
    bool m_passive = false;
    bool m_controls = true;
    bool m_inside = false;
    QRect m_hover;
    QRect m_sel;
    QPoint m_press;
    QRect m_selAtPress;
    bool m_dragged = false;
    Handle m_handle = None;
    QPoint m_cursor;
    qint64 m_copiedUntilMs = 0;
    QString m_lastCopied;
    bool m_pickClosing = false;

    Tool m_tool = Tool::None;
    QColor m_color;
    int m_sizeIdx[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    QList<Annotation> m_anns, m_redo;
    Annotation m_cur;
    bool m_drawing = false;

    ToolBar* m_bar;
    StyleBar* m_styleBar;
    QPlainTextEdit* m_editor;
    QPoint m_editorPos;
    bool m_editing = false;
};
