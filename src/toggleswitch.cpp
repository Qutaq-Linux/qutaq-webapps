#include "toggleswitch.h"

#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize ToggleSwitch::sizeHint() const { return {48, 26}; }

void ToggleSwitch::enterEvent(QEnterEvent *e) { m_hover = true; update(); QAbstractButton::enterEvent(e); }
void ToggleSwitch::leaveEvent(QEvent *e) { m_hover = false; update(); QAbstractButton::leaveEvent(e); }

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    const qreal radius = r.height() / 2;
    const QPalette &pal = palette();

    QColor track = isChecked() ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
    if (!isChecked())
        track.setAlpha(140);
    if (m_hover)
        track = track.lighter(110);
    if (!isEnabled())
        track.setAlpha(70);

    p.setPen(hasFocus() ? QPen(pal.color(QPalette::Highlight).lighter(130), 1.5) : Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(r, radius, radius);

    const qreal d = r.height() - 6;
    const qreal x = isChecked() ? r.right() - 3 - d : r.left() + 3;
    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? QColor(Qt::white) : pal.color(QPalette::Button));
    p.drawEllipse(QRectF(x, r.top() + 3, d, d));
}
