#include "timelinewidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <algorithm>

namespace { constexpr int kClipY = 32, kClipH = 56, kTextY = 98, kTextH = 24; }

TimelineWidget::TimelineWidget(Track* track, QVector<TextItem>* texts, QWidget* parent)
    : QWidget(parent), m_track(track), m_texts(texts) {
    setMinimumHeight(132);
    refresh();
}

void TimelineWidget::refresh() {
    double end = m_track->duration();
    for (const TextItem& t : *m_texts) end = std::max(end, t.start + t.duration);
    setMinimumWidth(std::max(800, timeToX(end) + 200));
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 34));

    const int step = m_pps >= 40 ? 1 : (m_pps >= 15 ? 5 : 15);
    p.setPen(QColor(150, 150, 150));
    for (int s = 0; timeToX(s) < width(); s += step) {
        p.drawLine(timeToX(s), 0, timeToX(s), 10);
        p.drawText(timeToX(s) + 3, 20, QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
    }

    const auto& clips = m_track->clips();
    double start = 0;
    for (int i = 0; i < clips.size(); ++i) {
        const Clip& c = clips[i];
        QRect r(timeToX(start), kClipY, std::max(4, int(c.duration() * m_pps) - 2), kClipH);
        p.fillRect(r, i == m_selected ? QColor(255, 170, 40) : QColor(60, 130, 220));
        if (i > 0 && c.transition != "None") p.fillRect(r.left() - 3, kClipY, 6, kClipH, QColor(255, 255, 255, 170));
        p.setPen(i == m_selected ? Qt::black : Qt::white);
        QString label = QFileInfo(c.path).fileName();
        if (c.speed != 1.0) label += QString("  %1x").arg(c.speed, 0, 'f', 2);
        if (c.effect != "None") label += "  [" + c.effect + "]";
        p.drawText(r.adjusted(6, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop, label);
        start += c.duration();
    }

    for (int i = 0; i < m_texts->size(); ++i) {
        const TextItem& t = (*m_texts)[i];
        QRect r(timeToX(t.start), kTextY, std::max(4, int(t.duration * m_pps) - 2), kTextH);
        p.fillRect(r, i == m_selText ? QColor(255, 170, 40) : QColor(150, 90, 220));
        p.setPen(i == m_selText ? Qt::black : Qt::white);
        p.drawText(r.adjusted(4, 0, -2, 0), Qt::AlignLeft | Qt::AlignVCenter, "T  " + t.text);
    }

    p.setPen(QPen(Qt::red, 2));
    p.drawLine(timeToX(m_playhead), 0, timeToX(m_playhead), height());
}

void TimelineWidget::pick(const QPoint& pos) {
    const double t = std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001));
    if (pos.y() >= kTextY - 4) {                       // text lane
        m_selText = -1;
        const double tt = xToTime(pos.x());
        for (int i = 0; i < m_texts->size(); ++i)
            if (tt >= (*m_texts)[i].start && tt <= (*m_texts)[i].start + (*m_texts)[i].duration) m_selText = i;
        if (m_selText >= 0) m_selected = -1;
    } else {
        m_selected = m_track->clipAt(t);
        m_selText = -1;
    }
    m_playhead = t;
    update();
    emit seekRequested(t);
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) { pick(e->position().toPoint()); }
void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) pick(e->position().toPoint());
}

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        m_pps = std::clamp(m_pps * (e->angleDelta().y() > 0 ? 1.25 : 0.8), 4.0, 400.0);
        refresh();
        e->accept();
    } else QWidget::wheelEvent(e);
}
