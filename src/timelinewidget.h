#pragma once
#include <QWidget>
#include "timeline.h"

// Draws the track as coloured blocks with a ruler and a red playhead.
// Click to seek/select. Ctrl + mouse wheel to zoom.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(Track* track, QWidget* parent = nullptr);
    void setPlayhead(double t) { m_playhead = t; update(); }
    double playhead() const { return m_playhead; }
    int selectedClip() const { return m_selected; }
    void clearSelection() { m_selected = -1; update(); }
    void refresh();   // call after the track changes

signals:
    void seekRequested(double t);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    double xToTime(int x) const { return (x - 10) / m_pps; }
    int timeToX(double t) const { return int(t * m_pps) + 10; }
    void pick(int x);

    Track* m_track;
    double m_playhead = 0;
    double m_pps = 40;      // pixels per second (zoom)
    int m_selected = -1;
};
