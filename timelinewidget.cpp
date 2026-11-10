#include "timelinewidget.h"
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <algorithm>

namespace {
constexpr int kLeft = 78, kRulerH = 26, kClipY = 40, kClipH = 62, kTextY = 112, kTextH = 30;
const QColor kAccent(34, 211, 238);
}

TimelineWidget::TimelineWidget(Track* track, QVector<TextItem>* texts, QWidget* parent)
    : QWidget(parent), m_track(track), m_texts(texts) {
    setMinimumHeight(156);
    refresh();
}

double TimelineWidget::xToTime(int x) const { return (x - kLeft) / m_pps; }
int TimelineWidget::timeToX(double t) const { return int(t * m_pps) + kLeft; }

void TimelineWidget::setPixelsPerSecond(double pps) {
    m_pps = std::clamp(pps, 4.0, 400.0);
    refresh();
    emit zoomChanged(m_pps);
}

void TimelineWidget::refresh() {
    double end = m_track->duration();
    for (const TextItem& t : *m_texts) end = std::max(end, t.start + t.duration);
    setMinimumWidth(std::max(900, timeToX(end) + 240));
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(14, 14, 19));

    // lanes
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(23, 23, 31));
    p.drawRoundedRect(QRect(kLeft, kClipY - 4, width() - kLeft - 6, kClipH + 8), 8, 8);
    p.setBrush(QColor(20, 20, 27));
    p.drawRoundedRect(QRect(kLeft, kTextY - 3, width() - kLeft - 6, kTextH + 6), 8, 8);

    // ruler
    const int step = m_pps >= 40 ? 1 : (m_pps >= 15 ? 5 : 15);
    QFont small = font(); small.setPixelSize(11); p.setFont(small);
    p.setPen(QColor(110, 110, 130));
    for (int s = 0; timeToX(s) < width(); s += step) {
        const int x = timeToX(s);
        p.drawLine(x, kRulerH - 8, x, kRulerH - 2);
        p.drawText(x + 4, kRulerH - 8, QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
    }
    p.setFont(font());

    // clips
    const auto& clips = m_track->clips();
    double start = 0;
    for (int i = 0; i < clips.size(); ++i) {
        const Clip& c = clips[i];
        const QRect r(timeToX(start), kClipY, std::max(6, int(c.duration() * m_pps) - 3), kClipH);
        const bool sel = (i == m_selected);
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0, sel ? QColor(76, 150, 255) : QColor(58, 112, 214));
        g.setColorAt(1, sel ? QColor(52, 104, 205) : QColor(40, 78, 170));
        p.setBrush(g);
        p.setPen(sel ? QPen(kAccent, 2) : QPen(QColor(90, 140, 235, 120), 1));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 8, 8);

        p.setPen(Qt::white);
        QString label = QFileInfo(c.path).fileName();
        QString meta = QString("%1s").arg(c.duration(), 0, 'f', 1);
        if (c.speed != 1.0) meta += QString("  %1x").arg(c.speed, 0, 'f', 2);
        if (c.effect != "None") meta += "  " + c.effect;
        QFontMetrics fm(font());
        p.drawText(r.adjusted(10, 8, -8, 0), Qt::AlignLeft | Qt::AlignTop, fm.elidedText(label, Qt::ElideRight, r.width() - 18));
        p.setPen(QColor(255, 255, 255, 170));
        p.setFont(small);
        p.drawText(r.adjusted(10, 0, -8, -8), Qt::AlignLeft | Qt::AlignBottom, fm.elidedText(meta, Qt::ElideRight, r.width() - 18));
        p.setFont(font());

        if (i > 0 && c.transition != "None") {           // transition diamond on the join
            const QPoint m(r.left(), kClipY + kClipH / 2);
            QPolygon d; d << QPoint(m.x(), m.y() - 9) << QPoint(m.x() + 9, m.y()) << QPoint(m.x(), m.y() + 9) << QPoint(m.x() - 9, m.y());
            p.setBrush(Qt::white); p.setPen(QPen(QColor(124, 58, 237), 2)); p.drawPolygon(d);
        }
        start += c.duration();
    }

    // text bars
    for (int i = 0; i < m_texts->size(); ++i) {
        const TextItem& t = (*m_texts)[i];
        const QRect r(timeToX(t.start), kTextY, std::max(6, int(t.duration * m_pps) - 3), kTextH);
        const bool sel = (i == m_selText);
        p.setBrush(sel ? QColor(167, 108, 250) : QColor(124, 58, 237));
        p.setPen(sel ? QPen(kAccent, 2) : Qt::NoPen);
        p.drawRoundedRect(r, 7, 7);
        p.setPen(Qt::white);
        QFontMetrics fm(font());
        p.drawText(r.adjusted(9, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText("T  " + t.text, Qt::ElideRight, r.width() - 14));
    }

    // left gutter with lane names (drawn last so it covers scrolled content edges)
    p.setPen(Qt::NoPen);
    p.fillRect(QRect(0, 0, kLeft - 6, height()), QColor(14, 14, 19));
    p.setPen(QColor(120, 120, 140));
    p.setFont(small);
    p.drawText(QRect(8, kClipY, kLeft - 20, kClipH), Qt::AlignLeft | Qt::AlignVCenter, "VIDEO");
    p.drawText(QRect(8, kTextY, kLeft - 20, kTextH), Qt::AlignLeft | Qt::AlignVCenter, "TEXT");
    p.setFont(font());

    // playhead
    const int px = timeToX(m_playhead);
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(px, 10, px, height());
    p.setBrush(Qt::white); p.setPen(Qt::NoPen);
    QPolygon head; head << QPoint(px - 7, 0) << QPoint(px + 7, 0) << QPoint(px, 12);
    p.drawPolygon(head);
}

void TimelineWidget::pick(const QPoint& pos, bool dragging) {
    const double t = std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001));
    if (!dragging) {                                        // selecting happens on click only
        const int oldC = m_selected, oldT = m_selText;
        if (pos.y() >= kTextY - 5) {
            m_selText = -1;
            const double tt = xToTime(pos.x());
            for (int i = 0; i < m_texts->size(); ++i)
                if (tt >= (*m_texts)[i].start && tt <= (*m_texts)[i].start + (*m_texts)[i].duration) m_selText = i;
            if (m_selText >= 0) m_selected = -1;
        } else if (pos.y() >= kClipY - 5) {
            m_selected = m_track->clipAt(t);
            m_selText = -1;
        }
        if (oldC != m_selected || oldT != m_selText) { m_playhead = t; emit selectionChanged(); }
    }
    m_playhead = t;
    update();
    emit seekRequested(t);
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) {
    if (e->position().x() < kLeft - 6) return;
    pick(e->position().toPoint(), false);
}
void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) pick(e->position().toPoint(), true);
}

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        setPixelsPerSecond(m_pps * (e->angleDelta().y() > 0 ? 1.25 : 0.8));
        e->accept();
    } else QWidget::wheelEvent(e);
}
