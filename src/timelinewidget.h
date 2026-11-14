#pragma once
#include <QWidget>
#include <QPoint>
#include <QRect>
#include "timeline.h"

class QDragEnterEvent;
class QDragMoveEvent;
class QDragLeaveEvent;
class QDropEvent;
class QContextMenuEvent;
class QMouseEvent;

// Draws the clips (blue), the FX lane (purple), transition blocks and text bars (violet)
// with a ruler and a playhead. Click to seek/select. Ctrl + mouse wheel to zoom.
// Effects and transitions can be dragged onto it from the side panels.
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
    // drag and drop from the Effects / Transitions panels
    void effectDropped(const QString& name, int clipIndex);
    void transitionDropped(const QString& name, int clipIndex);
    // right-click menus
    void removeEffectRequested(int clipIndex);
    void removeTransitionRequested(int clipIndex);
    // dragging the edge of a transition block (duration)
    void transitionEditStarted(int clipIndex);
    void transitionEditFinished();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dragMoveEvent(QDragMoveEvent*) override;
    void dragLeaveEvent(QDragLeaveEvent*) override;
    void dropEvent(QDropEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    double xToTime(int x) const;
    int timeToX(double t) const;
    void pick(const QPoint& pos, bool dragging);
    int hitTransition(const QPoint& pos) const;
    int hitEffect(const QPoint& pos) const;
    int nearestJoin(int x) const;
    QRect transitionRect(int index) const;

    Track* m_track;
    QVector<TextItem>* m_texts;
    double m_playhead = 0;
    double m_pps = 40;
    int m_selected = -1;
    int m_selText = -1;
    int m_hoverTrans = -1;       // transition under the cursor (for the tooltip)
    int m_dragClip = -1;         // clip highlighted while an effect is dragged over it
    int m_dragJoin = -1;         // join highlighted while a transition is dragged over it
    bool m_resizing = false;
    int m_resizeIndex = -1;
    int m_resizeStartX = 0;
    double m_resizeStartDur = 0.5;
};
