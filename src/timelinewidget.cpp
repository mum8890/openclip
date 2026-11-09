#include "timelinewidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <algorithm>

TimelineWidget::TimelineWidget(Track* track, QWidget* parent) : QWidget(parent), m_track(track) {
    setMinimumHeight(110);
    setMouseTracking(false);
    refresh();
}

void TimelineWidget::refresh() {
    setMinimumWidth(std::max(800, timeToX(m_track->duration()) + 200));
    update();
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(30, 30, 34));

    // ruler: a tick every N seconds depending on zoom
    const int step = m_pps >= 40 ? 1 : (m_pps >= 15 ? 5 : 15);
    p.setPen(QColor(150, 150, 150));
    for (int s = 0; timeToX(s) < width(); s += step) {
        p.drawLine(timeToX(s), 0, timeToX(s), 10);
        p.drawText(timeToX(s) + 3, 20, QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
    }

    // clips
    const auto& clips = m_track->clips();
    double start = 0;
    for (int i = 0; i < clips.size(); ++i) {
        const Clip& c = clips[i];
        QRect r(timeToX(start), 32, int(c.duration() * m_pps) - 2, 60);
        p.fillRect(r, i == m_selected ? QColor(255, 170, 40) : QColor(60, 130, 220));
        p.setPen(i == m_selected ? Qt::black : Qt::white);
        QString label = QFileInfo(c.path).fileName();
        if (c.speed != 1.0) label += QString("  (%1x)").arg(c.speed, 0, 'f', 2);
        p.drawText(r.adjusted(6, 4, -4, -4), Qt::AlignLeft | Qt::AlignTop, label);
        start += c.duration();
    }

    // playhead
    p.setPen(QPen(Qt::red, 2));
    p.drawLine(timeToX(m_playhead), 0, timeToX(m_playhead), height());
}

void TimelineWidget::pick(int x) {
    double t = std::clamp(xToTime(x), 0.0, std::max(0.0, m_track->duration() - 0.001));
    m_selected = m_track->clipAt(t);
    m_playhead = t;
    update();
    emit seekRequested(t);
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) { pick(e->position().toPoint().x()); }
void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) pick(e->position().toPoint().x());
}

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        m_pps = std::clamp(m_pps * (e->angleDelta().y() > 0 ? 1.25 : 0.8), 4.0, 400.0);
        refresh();
        e->accept();
    } else QWidget::wheelEvent(e);
}
