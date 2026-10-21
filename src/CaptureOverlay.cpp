#include "CaptureOverlay.h"
#include "PinWindow.h"
#include "Recognize.h"
#include "Settings.h"
#include "Style.h"
#include "ToolBars.h"
#include "WindowDetector.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>
#include <windows.h>

namespace {
constexpr int kMag = 132;
constexpr int kGrid = 13;
const qreal kWidths[3] = {2, 4, 7};
const qreal kMosaicWidths[3] = {14, 24, 40};
const qreal kNumberWidths[3] = {1, 3, 6};
const int kFontSizes[3] = {14, 20, 28};
}

CaptureOverlay::CaptureOverlay(QScreen* screen, const QImage& shot, const QRect& monitor, const WindowDetector* det)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_screen(screen), m_monitor(monitor), m_detector(det)
{
    m_raw = shot;
    m_raw.setDevicePixelRatio(1);
    m_dpr = shot.devicePixelRatio();
    m_shot = shot;
    m_canvas = QSizeF(m_raw.width() / m_dpr, m_raw.height() / m_dpr);
    const int block = qMax(6, int(10 * m_dpr));
    m_mosaic = m_raw.scaled(qMax(1, m_raw.width() / block), qMax(1, m_raw.height() / block),
                            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_color = Style::annotationColors().first();

    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    winId();
    if (windowHandle()) windowHandle()->setScreen(screen);
    setGeometry(screen->geometry());

    m_bar = new ToolBar(this);
    m_bar->hide();
    connect(m_bar, &ToolBar::triggered, this, [this](int id) {
        switch (id) {
        case ToolBar::TRect: setTool(Tool::Rect); break;
        case ToolBar::TEllipse: setTool(Tool::Ellipse); break;
        case ToolBar::TArrow: setTool(Tool::Arrow); break;
        case ToolBar::TPen: setTool(Tool::Pen); break;
        case ToolBar::TText: setTool(Tool::Text); break;
        case ToolBar::TMosaic: setTool(Tool::Mosaic); break;
        case ToolBar::TNumber: setTool(Tool::Number); break;
        case ToolBar::Undo: undo(); break;
        case ToolBar::Redo: redo(); break;
        case ToolBar::Pin: doPin(); break;
        case ToolBar::Save: doSave(false); break;
        case ToolBar::Cancel: emit finished(); break;
        case ToolBar::Confirm: doDefault(); break;
        case ToolBar::LongShot: doLongShot(); break;
        case ToolBar::OcrBtn: doRecognize(false); break;
        case ToolBar::QrBtn: doRecognize(true); break;
        }
        setFocus();
    });
    m_styleBar = new StyleBar(this);
    m_styleBar->hide();
    connect(m_styleBar, &StyleBar::sizeChanged, this, [this](int i) {
        m_sizeIdx[int(m_tool)] = i;
        setFocus();
    });
    connect(m_styleBar, &StyleBar::colorChanged, this, [this](const QColor& c) {
        m_color = c;
        if (m_editing) m_editor->setStyleSheet(editorStyle());
        setFocus();
    });

    m_editor = new QPlainTextEdit(this);
    m_editor->hide();
    m_editor->setFrameShape(QFrame::NoFrame);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_editor->document()->setDocumentMargin(0);
    m_editor->installEventFilter(this);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &CaptureOverlay::resizeEditor);
}

QString CaptureOverlay::editorStyle() const
{
    return QStringLiteral("QPlainTextEdit{background:rgba(255,255,255,30);border:1px dashed %1;color:%2;padding:0;}")
        .arg(Style::Accent.name(), m_color.name());
}

void CaptureOverlay::resizeEditor()
{
    const QFontMetrics fm(m_editor->font());
    const QStringList lines = m_editor->toPlainText().split(QLatin1Char('\n'));
    int w = 60;
    for (const QString& l : lines) w = qMax(w, fm.horizontalAdvance(l + QStringLiteral("  ")) + 12);
    const int h = fm.lineSpacing() * qMax<qsizetype>(1, lines.size()) + 8;
    m_editor->resize(qMin(w, width() - m_editor->x() - 2), h);
}

void CaptureOverlay::activate()
{
    show();
    raise();
    activateWindow();
    SetForegroundWindow(reinterpret_cast<HWND>(winId()));
    setFocus();
    const QPoint local = mapFromGlobal(QCursor::pos());
    if (rect().contains(local)) {
        m_inside = true;
        updateHover(local);
    }
}

void CaptureOverlay::setPassive(bool passive)
{
    m_passive = passive;
    if (passive && m_state != State::Idle) resetSelection();
    m_hover = {};
    update();
}

QPoint CaptureOverlay::toPhysical(const QPointF& l) const
{
    return QPoint(qBound(0, int(l.x() * m_dpr), m_raw.width() - 1), qBound(0, int(l.y() * m_dpr), m_raw.height() - 1));
}

QRect CaptureOverlay::toLogical(const QRect& r) const
{
    if (r.isEmpty()) return {};
    const QRectF f((r.x() - m_monitor.x()) / m_dpr, (r.y() - m_monitor.y()) / m_dpr, r.width() / m_dpr, r.height() / m_dpr);
    return f.toAlignedRect().intersected(rect());
}

QRect CaptureOverlay::physicalSelection() const
{
    const QRect r(qRound(m_sel.x() * m_dpr), qRound(m_sel.y() * m_dpr), qRound(m_sel.width() * m_dpr),
                  qRound(m_sel.height() * m_dpr));
    return r.intersected(m_raw.rect());
}

QList<QPointF> CaptureOverlay::handlePoints() const
{
    const QRectF r = m_sel;
    return {r.topLeft(), QPointF(r.center().x(), r.top()), r.topRight(), QPointF(r.right(), r.center().y()),
            r.bottomRight(), QPointF(r.center().x(), r.bottom()), r.bottomLeft(), QPointF(r.left(), r.center().y())};
}

CaptureOverlay::Handle CaptureOverlay::handleAt(const QPoint& p) const
{
    if (m_state != State::Selected) return None;
    const auto pts = handlePoints();
    static const Handle order[8] = {TL, T, TR, R, BR, B, BL, L};
    for (int i = 0; i < 8; ++i)
        if (QLineF(pts[i], p).length() <= 7) return order[i];
    if (m_tool == Tool::None && m_sel.contains(p)) return Move;
    return None;
}

void CaptureOverlay::updateHover(const QPoint& pos)
{
    if (m_state != State::Idle || m_passive || m_mode == CaptureMode::Pick) return;
    const QRect phys = m_detector->hitTest(m_monitor.topLeft() + toPhysical(pos), m_controls);
    QRect h = toLogical(phys);
    if (h.isEmpty()) h = rect();
    m_hover = h;
}

void CaptureOverlay::updateCursor(const QPoint& pos)
{
    if (m_state != State::Selected) {
        setCursor(Qt::CrossCursor);
        return;
    }
    switch (m_drawing ? None : handleAt(pos)) {
    case TL: case BR: setCursor(Qt::SizeFDiagCursor); break;
    case TR: case BL: setCursor(Qt::SizeBDiagCursor); break;
    case T: case B: setCursor(Qt::SizeVerCursor); break;
    case L: case R: setCursor(Qt::SizeHorCursor); break;
    case Move: setCursor(Qt::SizeAllCursor); break;
    default:
        if (m_tool == Tool::Text && m_sel.contains(pos)) setCursor(Qt::IBeamCursor);
        else setCursor(m_tool != Tool::None && m_sel.contains(pos) ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

qreal CaptureOverlay::currentWidth() const
{
    const int i = m_sizeIdx[int(m_tool)];
    if (m_tool == Tool::Mosaic) return kMosaicWidths[i];
    if (m_tool == Tool::Number) return kNumberWidths[i];
    return kWidths[i];
}

int CaptureOverlay::currentFontSize() const { return kFontSizes[m_sizeIdx[int(Tool::Text)]]; }

void CaptureOverlay::setTool(Tool t)
{
    if (m_editing) commitText();
    m_tool = (m_tool == t) ? Tool::None : t;
    static const int map[8] = {-1, ToolBar::TRect, ToolBar::TEllipse, ToolBar::TArrow, ToolBar::TPen,
                               ToolBar::TText, ToolBar::TMosaic, ToolBar::TNumber};
    m_bar->setActive(map[int(m_tool)]);
    applyStyleToBar();
    placeBars();
    updateCursor(mapFromGlobal(QCursor::pos()));
}

void CaptureOverlay::applyStyleToBar()
{
    if (m_tool == Tool::None) return;
    m_styleBar->configure(m_tool == Tool::Text, m_tool != Tool::Mosaic, m_sizeIdx[int(m_tool)], m_color);
}

void CaptureOverlay::placeBars()
{
    if (m_state != State::Selected || m_sel.isEmpty()) {
        m_bar->hide();
        m_styleBar->hide();
        return;
    }
    const QSize bv = m_bar->visualSize();
    const bool styled = m_tool != Tool::None;
    const QSize sv = m_styleBar->visualSize();
    const int extra = styled ? sv.height() + 8 : 0;
    int x = qBound(4, m_sel.right() - bv.width() + 1, qMax(4, width() - bv.width() - 4));
    int y = m_sel.bottom() + 14;
    bool above = false;
    if (y + bv.height() + extra > height() - 4) {
        y = m_sel.top() - 14 - bv.height();
        above = true;
        if (y - extra < 4) {
            y = m_sel.bottom() - bv.height() - 10;
            above = true;
        }
    }
    m_bar->move(x - ToolBar::Margin, y - ToolBar::Margin);
    m_bar->show();
    m_bar->raise();
    if (styled) {
        const int sx = qBound(4, x, qMax(4, width() - sv.width() - 4));
        const int sy = above ? y - 8 - sv.height() : y + bv.height() + 8;
        m_styleBar->move(sx - StyleBar::Margin, sy - StyleBar::Margin);
        m_styleBar->show();
        m_styleBar->raise();
    } else {
        m_styleBar->hide();
    }
}

void CaptureOverlay::resetSelection()
{
    cancelText();
    const bool had = m_state != State::Idle;
    m_state = State::Idle;
    m_sel = {};
    m_anns.clear();
    m_redo.clear();
    m_drawing = false;
    m_tool = Tool::None;
    m_bar->setActive(-1);
    refreshUndo();
    placeBars();
    updateHover(mapFromGlobal(QCursor::pos()));
    setCursor(Qt::CrossCursor);
    update();
    if (had) emit selectionCleared(this);
}

void CaptureOverlay::pushAnnotation(const Annotation& a)
{
    m_anns.append(a);
    m_redo.clear();
    refreshUndo();
}

void CaptureOverlay::undo()
{
    if (m_editing) {
        cancelText();
        return;
    }
    if (m_anns.isEmpty()) return;
    m_redo.append(m_anns.takeLast());
    refreshUndo();
    update();
}

void CaptureOverlay::redo()
{
    if (m_redo.isEmpty()) return;
    m_anns.append(m_redo.takeLast());
    refreshUndo();
    update();
}

void CaptureOverlay::refreshUndo() { m_bar->setUndoRedo(!m_anns.isEmpty(), !m_redo.isEmpty()); }

void CaptureOverlay::beginText(const QPoint& pos)
{
    m_editing = true;
    m_editorPos = pos;
    QFont f = annotationFont(currentFontSize());
    m_editor->setFont(f);
    m_editor->setStyleSheet(editorStyle());
    m_editor->clear();
    m_editor->move(pos.x() - 3, pos.y() - 3);
    resizeEditor();
    m_editor->show();
    m_editor->setFocus();
}

void CaptureOverlay::commitText()
{
    if (!m_editing) return;
    m_editing = false;
    QString text = m_editor->toPlainText();
    while (text.endsWith(QLatin1Char('\n'))) text.chop(1);
    m_editor->hide();
    if (!text.trimmed().isEmpty()) {
        Annotation a;
        a.type = Tool::Text;
        a.color = m_color;
        a.p1 = m_editorPos;
        a.text = text;
        a.fontSize = currentFontSize();
        pushAnnotation(a);
    }
    setFocus();
    update();
}

void CaptureOverlay::cancelText()
{
    if (!m_editing) return;
    m_editing = false;
    m_editor->hide();
    setFocus();
}

bool CaptureOverlay::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_editor && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        // Enter / Shift+Enter 换行，Ctrl+Enter 完成
        if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter) && (k->modifiers() & Qt::ControlModifier)) {
            commitText();
            return true;
        }
        if (k->key() == Qt::Key_Escape) {
            cancelText();
            return true;
        }
    }
    if (o == m_editor && e->type() == QEvent::FocusOut && m_editing) {
        QMetaObject::invokeMethod(this, &CaptureOverlay::commitText, Qt::QueuedConnection);
    }
    return QWidget::eventFilter(o, e);
}

QImage CaptureOverlay::renderResult() const
{
    QImage full = m_raw.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    {
        QPainter p(&full);
        p.scale(m_dpr, m_dpr);
        for (const Annotation& a : m_anns) paintAnnotation(p, a, m_mosaic, m_canvas);
    }
    QImage out = full.copy(physicalSelection()).convertToFormat(QImage::Format_RGB32);
    out.setDevicePixelRatio(m_dpr);
    return out;
}

void CaptureOverlay::doCopy()
{
    if (m_state != State::Selected) return;
    if (m_editing) commitText();
    QImage img = renderResult();
    img.setDevicePixelRatio(1);
    QGuiApplication::clipboard()->setImage(img);
    emit delivered(img, QString());
    emit finished();
}

void CaptureOverlay::doSave(bool saveAs)
{
    if (m_state != State::Selected) return;
    if (m_editing) commitText();
    const QImage img = renderResult();
    QString path = Settings::defaultFilePath();
    if (saveAs) {
        const bool jpg = Settings::format() == QLatin1String("jpg");
        const QString png = QStringLiteral("PNG 图片 (*.png)"), jpgf = QStringLiteral("JPG 图片 (*.jpg *.jpeg)");
        QString filter = jpg ? jpgf : png;
        QDir().mkpath(Settings::saveDir());
        path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为"), path, png + QStringLiteral(";;") + jpgf, &filter);
        setFocus();
        if (path.isEmpty()) return;
    }
    if (!Settings::saveImage(img, path)) {
        QMessageBox::warning(this, QStringLiteral("青影"), QStringLiteral("保存失败：%1").arg(path));
        return;
    }
    emit delivered(img, path);
    emit finished();
}

void CaptureOverlay::doPin()
{
    if (m_state != State::Selected) return;
    if (m_editing) commitText();
    const QImage img = renderResult();
    emit delivered(img, QString());
    auto* pin = new PinWindow(img);
    pin->placeImageAt(mapToGlobal(m_sel.topLeft()));
    pin->show();
    pin->activateWindow();
    emit finished();
}

void CaptureOverlay::doDefault()
{
    if (m_mode == CaptureMode::LongShot) return doLongShot();
    if (m_mode == CaptureMode::Record) {
        if (m_state != State::Selected) return;
        QRect phys = physicalSelection().translated(m_monitor.topLeft());
        phys.setWidth(phys.width() & ~1);
        phys.setHeight(phys.height() & ~1);
        if (phys.width() < 16 || phys.height() < 16) return;
        emit recordRequested(phys);
        emit finished();
        return;
    }
    const QString a = Settings::defaultAction();
    if (a == QLatin1String("save")) doSave(false);
    else if (a == QLatin1String("pin")) doPin();
    else doCopy();
}

void CaptureOverlay::doRecognize(bool qr)
{
    if (m_state != State::Selected) return;
    if (m_editing) commitText();
    const QImage img = renderResult();
    emit finished();
    if (qr) Recognize::qr(img);
    else Recognize::ocr(img);
}

void CaptureOverlay::doLongShot()
{
    if (m_state != State::Selected) return;
    const QRect phys = physicalSelection().translated(m_monitor.topLeft());
    if (phys.height() < 60 || phys.width() < 60) {
        QMessageBox::information(this, QStringLiteral("长截图"), QStringLiteral("选区太小，请至少选择 60×60 像素。"));
        return;
    }
    emit longShotRequested(phys);
    emit finished();
}

void CaptureOverlay::copyHex()
{
    const QString text = Settings::formatColor(m_raw.pixelColor(toPhysical(m_cursor)));
    QGuiApplication::clipboard()->setText(text);
    emit colorPicked(text);
    if (m_mode == CaptureMode::Pick) emit finished();
}

bool CaptureOverlay::magnifierVisible() const
{
    if (m_passive || !m_inside) return false;
    if (m_state == State::Idle) return true;
    if (m_state == State::Selecting) return true;
    return m_handle != None && m_handle != Move && !m_drawing;
}

// ---------------- 事件 ----------------

void CaptureOverlay::mousePressEvent(QMouseEvent* e)
{
    const QPoint pos = e->position().toPoint();
    m_cursor = pos;
    if (e->button() == Qt::RightButton) {
        // 右键：编辑文字时取消输入，否则退出截图
        if (m_editing) cancelText();
        else if (!m_drawing) emit finished();
        return;
    }
    if (e->button() != Qt::LeftButton) return;
    if (m_mode == CaptureMode::Pick) {
        copyHex();
        return;
    }
    if (m_editing) {
        commitText();
        if (m_tool == Tool::Text) return;
    }
    if (m_passive) {
        m_passive = false;
    }
    m_press = pos;
    m_dragged = false;
    if (m_state == State::Idle) {
        m_state = State::Selecting;
        emit selectionStarted(this);
        update();
        return;
    }
    if (m_state != State::Selected) return;

    m_handle = handleAt(pos);
    if (m_handle != None && !(m_tool != Tool::None && m_handle == Move)) {
        m_selAtPress = m_sel;
        return;
    }
    m_handle = None;
    if (m_tool != Tool::None && m_sel.contains(pos)) {
        if (m_tool == Tool::Text) {
            beginText(pos);
            return;
        }
        if (m_tool == Tool::Number) {
            Annotation a;
            a.type = Tool::Number;
            a.color = m_color;
            a.width = currentWidth();
            a.p1 = pos;
            int n = 1;
            for (const Annotation& x : m_anns)
                if (x.type == Tool::Number) n = qMax(n, x.number + 1);
            a.number = n;
            pushAnnotation(a);
            update();
            return;
        }
        m_drawing = true;
        m_cur = Annotation();
        m_cur.type = m_tool;
        m_cur.color = m_color;
        m_cur.width = currentWidth();
        m_cur.p1 = m_cur.p2 = pos;
        m_cur.points = {QPointF(pos)};
        update();
        return;
    }
    // 没有标注时，在选区外拖动可重新框选
    if (m_anns.isEmpty() && m_tool == Tool::None && !m_sel.contains(pos)) {
        m_state = State::Selecting;
        m_sel = {};
        placeBars();
        update();
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint pos = e->position().toPoint();
    m_cursor = pos;
    m_inside = true;
    if (m_mode == CaptureMode::Pick) {
        update();
        return;
    }
    if (m_state == State::Idle) {
        updateHover(pos);
    } else if (m_state == State::Selecting && (e->buttons() & Qt::LeftButton)) {
        if ((pos - m_press).manhattanLength() > 3) m_dragged = true;
        if (m_dragged) m_sel = QRect(m_press, pos).normalized().intersected(rect());
    } else if (m_state == State::Selected && (e->buttons() & Qt::LeftButton)) {
        if (m_drawing) {
            QPointF p = pos;
            p.setX(qBound<qreal>(m_sel.left(), p.x(), m_sel.right()));
            p.setY(qBound<qreal>(m_sel.top(), p.y(), m_sel.bottom()));
            if ((m_cur.type == Tool::Rect || m_cur.type == Tool::Ellipse) && (e->modifiers() & Qt::ShiftModifier)) {
                const qreal d = qMax(qAbs(p.x() - m_cur.p1.x()), qAbs(p.y() - m_cur.p1.y()));
                p = m_cur.p1 + QPointF(p.x() >= m_cur.p1.x() ? d : -d, p.y() >= m_cur.p1.y() ? d : -d);
            }
            m_cur.p2 = p;
            if (m_cur.type == Tool::Pen || m_cur.type == Tool::Mosaic) m_cur.points.append(p);
        } else if (m_handle != None) {
            QRect r = m_selAtPress;
            const QPoint d = pos - m_press;
            switch (m_handle) {
            case Move: {
                r.translate(d);
                r.moveLeft(qBound(0, r.left(), width() - r.width()));
                r.moveTop(qBound(0, r.top(), height() - r.height()));
                break;
            }
            case TL: r.setTopLeft(r.topLeft() + d); break;
            case T: r.setTop(r.top() + d.y()); break;
            case TR: r.setTopRight(r.topRight() + d); break;
            case R: r.setRight(r.right() + d.x()); break;
            case BR: r.setBottomRight(r.bottomRight() + d); break;
            case B: r.setBottom(r.bottom() + d.y()); break;
            case BL: r.setBottomLeft(r.bottomLeft() + d); break;
            case L: r.setLeft(r.left() + d.x()); break;
            default: break;
            }
            m_sel = r.normalized().intersected(rect());
            placeBars();
        }
    }
    if (!(e->buttons() & Qt::LeftButton)) updateCursor(pos);
    update();
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    if (m_state == State::Selecting) {
        if (!m_dragged || m_sel.width() < 2 || m_sel.height() < 2) m_sel = m_hover.isEmpty() ? rect() : m_hover;
        m_state = State::Selected;
        m_hover = {};
        placeBars();
    } else if (m_drawing) {
        m_drawing = false;
        if (m_cur.isValid()) pushAnnotation(m_cur);
    }
    m_handle = None;
    updateCursor(e->position().toPoint());
    update();
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_state == State::Selected && m_tool == Tool::None &&
        m_sel.contains(e->position().toPoint()))
        doDefault();
}

void CaptureOverlay::keyPressEvent(QKeyEvent* e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    switch (e->key()) {
    case Qt::Key_Escape: emit finished(); return;
    case Qt::Key_Return:
    case Qt::Key_Enter: doDefault(); return;
    case Qt::Key_Tab:
        m_controls = !m_controls;
        updateHover(m_cursor);
        update();
        return;
    case Qt::Key_F3: doPin(); return;
    default: break;
    }
    if (ctrl && e->key() == Qt::Key_S) return doSave(e->modifiers() & Qt::ShiftModifier);
    if (ctrl && e->key() == Qt::Key_C) return m_state == State::Selected ? doCopy() : copyHex();
    if (ctrl && (e->key() == Qt::Key_Y || (e->key() == Qt::Key_Z && (e->modifiers() & Qt::ShiftModifier)))) return redo();
    if (ctrl && e->key() == Qt::Key_Z) return undo();
    if (!ctrl && e->key() == Qt::Key_C) return copyHex();
    if (m_state == State::Selected && !ctrl) {
        switch (e->key()) {
        case Qt::Key_R: return setTool(Tool::Rect);
        case Qt::Key_E: return setTool(Tool::Ellipse);
        case Qt::Key_A: return setTool(Tool::Arrow);
        case Qt::Key_P: return setTool(Tool::Pen);
        case Qt::Key_T: return setTool(Tool::Text);
        case Qt::Key_M: return setTool(Tool::Mosaic);
        case Qt::Key_N: return setTool(Tool::Number);
        case Qt::Key_L: return doLongShot();
        case Qt::Key_O: return doRecognize(false);
        case Qt::Key_Q: return doRecognize(true);
        default: break;
        }
    }
    // 方向键微调光标
    QPoint d;
    if (e->key() == Qt::Key_Left) d = {-1, 0};
    if (e->key() == Qt::Key_Right) d = {1, 0};
    if (e->key() == Qt::Key_Up) d = {0, -1};
    if (e->key() == Qt::Key_Down) d = {0, 1};
    if (!d.isNull()) QCursor::setPos(QCursor::pos() + d);
}

void CaptureOverlay::leaveEvent(QEvent*)
{
    m_inside = false;
    if (m_state == State::Idle) m_hover = {};
    update();
}

// ---------------- 绘制 ----------------

void CaptureOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.drawImage(QPointF(0, 0), m_shot);

    QRect hole;
    if (m_state == State::Idle) hole = m_passive ? QRect() : m_hover;
    else if (m_state == State::Selecting) hole = m_dragged ? m_sel : m_hover;
    else hole = m_sel;

    QRegion dim(rect());
    if (!hole.isEmpty()) dim -= QRegion(hole);
    p.setClipRegion(dim);
    p.fillRect(rect(), Style::Overlay);
    p.setClipping(false);
    p.setRenderHint(QPainter::Antialiasing);

    if (!hole.isEmpty()) {
        const bool detecting = m_state == State::Idle || (m_state == State::Selecting && !m_dragged);
        if (detecting) {
            p.fillRect(hole, QColor(79, 107, 255, 15));
            p.setPen(QPen(Style::Accent, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(hole).adjusted(1, 1, -1, -1));
            const QSize ps = (QSizeF(hole.size()) * m_dpr).toSize();
            const QString label = QStringLiteral("%1  %2 × %3").arg(m_controls ? QStringLiteral("窗口/控件") : QStringLiteral("窗口"))
                                      .arg(ps.width()).arg(ps.height());
            const QSizeF sz = Style::pillSize(label);
            QPointF at(hole.left() + 8, hole.top() + 8);
            Style::paintPill(p, at, label);
            if (hole.height() > 120) {
                const QString tip = QStringLiteral("单击选中 · 拖动框选 · Tab 切换控件识别");
                const QSizeF ts = Style::pillSize(tip);
                Style::paintPill(p, QPointF(hole.left() + 8, at.y() + sz.height() + 6), tip).width();
                Q_UNUSED(ts);
            }
        } else {
            p.save();
            p.setClipRect(m_sel);
            for (const Annotation& a : m_anns) paintAnnotation(p, a, m_mosaic, m_canvas);
            if (m_drawing) paintAnnotation(p, m_cur, m_mosaic, m_canvas);
            p.restore();
            p.setPen(QPen(Style::Accent, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(hole).adjusted(0.75, 0.75, -0.75, -0.75));
            if (m_state == State::Selected) {
                for (const QPointF& h : handlePoints()) {
                    p.setPen(QPen(Qt::white, 1.5));
                    p.setBrush(Style::Accent);
                    p.drawEllipse(h, 4.5, 4.5);
                }
            }
            const QRect phys = physicalSelection();
            const QString label = QStringLiteral("%1 × %2").arg(phys.width()).arg(phys.height());
            const QSizeF sz = Style::pillSize(label);
            QPointF at(hole.left(), hole.top() - sz.height() - 8);
            if (at.y() < 4) at = QPointF(hole.left() + 8, hole.top() + 8);
            Style::paintPill(p, at, label);
        }
    }
    if (m_mode != CaptureMode::Normal && m_inside) {
        static const QString tips[] = {QString(), QStringLiteral("长截图：框选要滚动的区域后按 Enter 开始"),
                                       QStringLiteral("取色：单击或按 C 复制颜色 · Esc 退出"),
                                       QStringLiteral("录屏：框选录制区域后按 Enter 开始")};
        const QString tip = tips[int(m_mode)];
        const QSizeF ts = Style::pillSize(tip);
        Style::paintPill(p, QPointF((width() - ts.width()) / 2, 24), tip);
    }
    if (m_bar->isVisible()) Style::paintShadow(p, QRectF(m_bar->geometry()), 14);
    if (m_styleBar->isVisible()) Style::paintShadow(p, QRectF(m_styleBar->geometry()), 12);
    if (magnifierVisible()) paintMagnifier(p);
}

void CaptureOverlay::paintMagnifier(QPainter& p)
{
    const QPoint c = m_cursor;
    const QPoint phys = toPhysical(c);
    const QImage patch = m_raw.copy(phys.x() - kGrid / 2, phys.y() - kGrid / 2, kGrid, kGrid);
    const QColor col = m_raw.pixelColor(phys);
    const int infoH = 46;
    QPoint tl = c + QPoint(20, 20);
    if (tl.x() + kMag > width() - 4) tl.setX(c.x() - 20 - kMag);
    if (tl.y() + kMag + infoH + 8 > height() - 4) tl.setY(c.y() - 20 - kMag - infoH - 8);
    const QRectF circle(tl, QSizeF(kMag, kMag));

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    Style::paintShadow(p, circle, kMag / 2.0, 8);
    QPainterPath clip;
    clip.addEllipse(circle);
    p.setClipPath(clip);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(circle, patch);
    const qreal cell = qreal(kMag) / kGrid;
    p.setPen(QPen(QColor(255, 255, 255, 38), 1));
    for (int i = 1; i < kGrid; ++i) {
        p.drawLine(QPointF(circle.left() + i * cell, circle.top()), QPointF(circle.left() + i * cell, circle.bottom()));
        p.drawLine(QPointF(circle.left(), circle.top() + i * cell), QPointF(circle.right(), circle.top() + i * cell));
    }
    p.setPen(QPen(Style::Accent, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(circle.left() + (kGrid / 2) * cell, circle.top() + (kGrid / 2) * cell, cell, cell));
    p.setClipping(false);
    p.setPen(QPen(Qt::white, 3));
    p.drawEllipse(circle.adjusted(1.5, 1.5, -1.5, -1.5));
    p.setPen(QPen(QColor(0, 0, 0, 40), 1));
    p.drawEllipse(circle);

    const QRectF info(circle.left(), circle.bottom() + 8, kMag, infoH);
    p.setPen(Qt::NoPen);
    p.setBrush(Style::Pill);
    p.drawRoundedRect(info, 8, 8);
    p.setBrush(col);
    p.setPen(QPen(QColor(255, 255, 255, 120), 1));
    p.drawRoundedRect(QRectF(info.left() + 10, info.top() + 9, 12, 12), 3, 3);
    const QString colorText = Settings::formatColor(col);
    QFont mono(QStringLiteral("Consolas"));
    mono.setPixelSize(12);
    p.setFont(mono);
    p.setPen(Qt::white);
    p.drawText(QRectF(info.left() + 30, info.top() + 6, kMag - 36, 18), Qt::AlignVCenter | Qt::AlignLeft, colorText.left(14));
    QFont small(Style::uiFontFamily());
    small.setPixelSize(11);
    p.setFont(small);
    p.setPen(Style::Text3);
    p.drawText(QRectF(info.left() + 30, info.top() + 6, kMag - 40, 18), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("C 复制"));
    p.setFont(mono);
    p.setPen(QColor(255, 255, 255, 200));
    p.drawText(QRectF(info.left() + 10, info.top() + 24, kMag - 20, 18), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("RGB %1, %2, %3").arg(col.red()).arg(col.green()).arg(col.blue()));
    p.restore();
}
