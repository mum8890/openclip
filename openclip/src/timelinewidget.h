#pragma once
#include <QWidget>
#include "timeline.h"

// Draws the clips (blue) and text bars (purple) with a ruler and a playhead.
// Click to seek/select. Ctrl + mouse wheel to zoom.
class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    TimelineWidget(Track* track, QVector<TextItem>* texts, QWidget* parent = nullptr);
    void setPlayhead(double t) { m_playhead = t; update(); }
    double playhead() const { return m_playhead; }
    int selectedClip() const { return m_selected; }
    int selectedText() const { return m_selText; }
    void selectClip(int i) { m_selected = i; m_selText = -1; update(); emit selectionChanged(); }
    void selectText(int i) { m_selText = i; m_selected = -1; update(); emit selectionChanged(); }
    void clearSelection() { m_selected = -1; m_selText = -1; update(); emit selectionChanged(); }
    double pixelsPerSecond() const { return m_pps; }
    void setPixelsPerSecond(double pps);
    void refresh();

signals:
    void seekRequested(double t);
    void selectionChanged();
    void zoomChanged(double pps);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

private:
    double xToTime(int x) const;
    int timeToX(double t) const;
    void pick(const QPoint& pos, bool dragging);

    Track* m_track;
    QVector<TextItem>* m_texts;
    double m_playhead = 0;
    double m_pps = 40;
    int m_selected = -1;
    int m_selText = -1;
};
