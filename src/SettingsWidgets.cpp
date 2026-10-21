#include "SettingsWidgets.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QVariantAnimation>

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent), m_anim(new QVariantAnimation(this))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    m_anim->setDuration(140);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_pos = v.toDouble();
        update();
    });
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        m_anim->stop();
        if (!isVisible()) {
            m_pos = on ? 1 : 0;
            update();
            return;
        }
        m_anim->setStartValue(m_pos);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
    });
}

QSize ToggleSwitch::sizeHint() const { return {40, 22}; }

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    if (!m_anim->state()) m_pos = isChecked() ? 1 : 0;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(0, 0, 40, 22).translated((width() - 40) / 2.0, (height() - 22) / 2.0);
    const QColor off = Theme::isDark() ? QColor(0x4A, 0x4B, 0x52) : QColor(0xD5, 0xD7, 0xDE);
    const QColor on(0x4F, 0x6B, 0xFF);
    auto mix = [](const QColor& a, const QColor& b, double t) {
        return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t), float(a.greenF() + (b.greenF() - a.greenF()) * t),
                                float(a.blueF() + (b.blueF() - a.blueF()) * t));
    };
    QColor c = mix(off, on, m_pos);
    if (!isEnabled()) c.setAlphaF(0.4f);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRoundedRect(track, 11, 11);
    if (hasFocus()) {
        p.setPen(QPen(QColor(79, 107, 255, 110), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(track.adjusted(-1, -1, 1, 1), 12, 12);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 30));
    const double x = track.left() + 3 + m_pos * 18;
    p.drawEllipse(QRectF(x, track.top() + 3.5, 16, 16));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(x, track.top() + 3, 16, 16));
}

Segmented::Segmented(const QStringList& labels, QWidget* parent) : QFrame(parent), m_group(new QButtonGroup(this))
{
    setObjectName(QStringLiteral("seg"));
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(3, 3, 3, 3);
    h->setSpacing(2);
    for (int i = 0; i < labels.size(); ++i) {
        auto* b = new QPushButton(labels[i], this);
        b->setObjectName(QStringLiteral("segBtn"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_group->addButton(b, i);
        h->addWidget(b);
    }
    connect(m_group, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (on) emit changed(id);
    });
}

int Segmented::current() const { return m_group->checkedId(); }

void Segmented::setCurrent(int i)
{
    if (QAbstractButton* b = m_group->button(i)) {
        const QSignalBlocker block(m_group);
        b->setChecked(true);
    }
}

HotkeyEdit::HotkeyEdit(QWidget* parent) : QPushButton(parent)
{
    setObjectName(QStringLiteral("hotkey"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(180);
    connect(this, &QPushButton::clicked, this, [this] { setRecording(!m_recording); });
    refresh();
}

void HotkeyEdit::setSequence(const QKeySequence& k)
{
    m_seq = k;
    refresh();
}

void HotkeyEdit::setRecording(bool on)
{
    m_recording = on;
    setProperty("recording", on);
    style()->unpolish(this);
    style()->polish(this);
    if (on) setFocus();
    refresh();
}

void HotkeyEdit::refresh(Qt::KeyboardModifiers partial)
{
    if (m_recording) {
        QString t = QKeySequence(int(partial.toInt())).toString(QKeySequence::NativeText);
        setText(t.isEmpty() ? QCoreApplication::translate("QingYing", "请按下组合键…（Esc 取消）") : t + QStringLiteral("…"));
    } else {
        setText(m_seq.isEmpty() ? QCoreApplication::translate("QingYing", "未设置") : m_seq.toString(QKeySequence::NativeText));
    }
}

void HotkeyEdit::keyPressEvent(QKeyEvent* e)
{
    if (!m_recording) {
        if (e->key() == Qt::Key_Space || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) setRecording(true);
        else QPushButton::keyPressEvent(e);
        return;
    }
    const int key = e->key();
    if (key == Qt::Key_Escape && e->modifiers() == Qt::NoModifier) {
        setRecording(false);
        return;
    }
    if ((key == Qt::Key_Backspace || key == Qt::Key_Delete) && e->modifiers() == Qt::NoModifier) {
        setRecording(false);
        emit recorded(QKeySequence());
        return;
    }
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta || key == Qt::Key_unknown) {
        refresh(e->modifiers());
        return;
    }
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    setRecording(false);
    emit recorded(QKeySequence(QKeyCombination(mods, Qt::Key(key))));
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent* e)
{
    if (m_recording) refresh(e->modifiers());
    else QPushButton::keyReleaseEvent(e);
}

void HotkeyEdit::focusOutEvent(QFocusEvent* e)
{
    if (m_recording) setRecording(false);
    QPushButton::focusOutEvent(e);
}
