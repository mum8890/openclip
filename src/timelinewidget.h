#pragma once
#include <QWidget>
#include "timeline.h"

// Draws the clips (blue) and text bars (purple) with a ruler and a red playhead.
// Click to seek/select. Ctrl + mouse wheel to zoom.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    TimelineWidget(Track* track, QVector<TextItem>* texts, QWidget* parent = nullptr);
    void setPlayhead(double t) { m_playhead = t; update(); }
    double playhead() const { return m_playhead; }
    int selectedClip() const { return m_selected; }
    int selectedText() const { return m_selText; }
    void clearSelection() { m_selected = -1; m_selText = -1; update(); }
    void refresh();

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
    void pick(const QPoint& pos);

    Track* m_track;
    QVector<TextItem>* m_texts;
    double m_playhead = 0;
    double m_pps = 40;
    int m_selected = -1;
    int m_selText = -1;
};
