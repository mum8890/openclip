#include "timelinewidget.h"
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kLeft = 78, kRulerH = 26, kClipY = 40, kClipH = 62;
constexpr int kFxY = 108, kFxH = 24, kTextY = 140, kTextH = 30, kMinH = 182;
const QColor kAccent(34, 211, 238);
const char* kEffectMime = "application/x-openclip-effect";
const char* kTransMime  = "application/x-openclip-transition";
}

TimelineWidget::TimelineWidget(Track* track, QVector<TextItem>* texts, QWidget* parent)
    : QWidget(parent), m_track(track), m_texts(texts) {
    setMinimumHeight(kMinH);
    setAcceptDrops(true);
    setMouseTracking(true);
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
    setMinimumHeight(kMinH);
    update();
}

QRect TimelineWidget::transitionRect(int index) const {
    const auto& clips = m_track->clips();
    if (index <= 0 || index >= clips.size() || clips[index].transition == "None") return {};
    const double ov = m_track->joinOverlap(index);
    if (ov <= 0.001) return {};
    const int x = timeToX(m_track->clipStart(index));
    const int w = std::max(30, int(ov * m_pps));
    const int h = 24;
    return QRect(x - w / 2, kClipY + (kClipH - h) / 2, w, h);
}

int TimelineWidget::hitTransition(const QPoint& pos) const {
    for (int i = 1; i < m_track->clips().size(); ++i) {
        const QRect r = transitionRect(i);
        if (!r.isEmpty() && r.contains(pos)) return i;
    }
    return -1;
}

int TimelineWidget::hitEffect(const QPoint& pos) const {
    if (pos.y() < kFxY - 4 || pos.y() > kFxY + kFxH + 4) return -1;
    const auto& clips = m_track->clips();
    for (int i = 0; i < clips.size(); ++i) {
        if (clips[i].effect == "None") continue;
        const QRect r(timeToX(m_track->clipStart(i)), kFxY,
                      std::max(6, int(clips[i].duration() * m_pps) - 3), kFxH);
        if (r.contains(pos)) return i;
    }
    return -1;
}

int TimelineWidget::nearestJoin(int x) const {
    int best = -1, bestD = 34;
    for (int i = 1; i < m_track->clips().size(); ++i) {
        const int d = std::abs(x - timeToX(m_track->clipStart(i)));
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient backdrop(0, 0, 0, height());
    backdrop.setColorAt(0, QColor(16, 15, 29));
    backdrop.setColorAt(1, QColor(10, 10, 20));
    p.fillRect(rect(), backdrop);

    // lanes
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(27, 26, 43));
    p.drawRoundedRect(QRect(kLeft, kClipY - 4, width() - kLeft - 6, kClipH + 8), 8, 8);
    p.setBrush(QColor(32, 27, 50));
    p.drawRoundedRect(QRect(kLeft, kFxY - 3, width() - kLeft - 6, kFxH + 6), 8, 8);
    p.setBrush(QColor(24, 23, 38));
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

    // clips (transitions overlap the neighbouring bars, like in the exported video)
    const auto& clips = m_track->clips();
    for (int i = 0; i < clips.size(); ++i) {
        const Clip& c = clips[i];
        const double start = m_track->clipStart(i);
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
    }

    // FX lane: one block per clip that has a look, so the look is a separate item you can see
    QFontMetrics fxMetrics(font());
    for (int i = 0; i < clips.size(); ++i) {
        const Clip& c = clips[i];
        if (c.effect == "None") continue;
        const QRect r(timeToX(m_track->clipStart(i)), kFxY,
                      std::max(6, int(c.duration() * m_pps) - 3), kFxH);
        const bool sel = (i == m_selected);
        QLinearGradient g(r.topLeft(), r.topRight());
        g.setColorAt(0, sel ? QColor(167, 139, 250) : QColor(124, 58, 237));
        g.setColorAt(1, sel ? QColor(244, 114, 182) : QColor(190, 60, 150));
        p.setBrush(g);
        p.setPen(sel ? QPen(kAccent, 2) : QPen(QColor(196, 160, 255, 120), 1));
        p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 6, 6);
        p.setPen(Qt::white);
        p.setFont(small);
        p.drawText(r.adjusted(8, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter,
                   fxMetrics.elidedText("FX  " + c.effect, Qt::ElideRight, r.width() - 12));
        p.setFont(font());
    }

    // transition blocks straddling each join (drag the edge to change the length)
    for (int i = 1; i < clips.size(); ++i) {
        const QRect r = transitionRect(i);
        if (r.isEmpty()) continue;
        QLinearGradient g(r.topLeft(), r.topRight());
        g.setColorAt(0, QColor(34, 211, 238));
        g.setColorAt(1, QColor(139, 92, 246));
        p.setBrush(g);
        const bool hot = (i == m_hoverTrans || i == m_dragJoin);
        p.setPen(hot ? QPen(Qt::white, 2) : QPen(QColor(230, 240, 255, 180), 1));
        p.drawRoundedRect(r, 7, 7);
        p.setPen(Qt::white);
        QFont tf = font(); tf.setPixelSize(10); tf.setBold(true); p.setFont(tf);
        if (r.width() >= 52) {
            QFontMetrics tfm(tf);
            p.drawText(r.adjusted(8, 0, -6, 0), Qt::AlignCenter,
                       tfm.elidedText(transitionLabel(clips[i].transition), Qt::ElideRight, r.width() - 14));
        } else {
            p.drawText(r, Qt::AlignCenter, QString::fromUtf8("\xE2\x87\x84"));
        }
        p.setFont(font());
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

    // drag feedback from the side panels
    if (m_dragClip >= 0 && m_dragClip < clips.size()) {
        const QRect r(timeToX(m_track->clipStart(m_dragClip)), kClipY,
                      std::max(6, int(clips[m_dragClip].duration() * m_pps) - 3), kClipH);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kAccent, 3));
        p.drawRoundedRect(r.adjusted(-2, -2, 2, 2), 10, 10);
    }
    if (m_dragJoin > 0 && m_dragJoin < clips.size()) {
        QRect r = transitionRect(m_dragJoin);
        if (r.isEmpty()) {
            const int x = timeToX(m_track->clipStart(m_dragJoin));
            r = QRect(x - 16, kClipY + kClipH / 2 - 16, 32, 32);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kAccent, 3));
        p.drawRoundedRect(r.adjusted(-3, -3, 3, 3), 10, 10);
    }

    // left gutter with lane names (drawn last so it covers scrolled content edges)
    p.setPen(Qt::NoPen);
    p.fillRect(QRect(0, 0, kLeft - 6, height()), QColor(13, 13, 24));
    p.setPen(QColor(120, 120, 140));
    p.setFont(small);
    p.drawText(QRect(8, kClipY, kLeft - 20, kClipH), Qt::AlignLeft | Qt::AlignVCenter, "VIDEO");
    p.drawText(QRect(8, kFxY, kLeft - 20, kFxH), Qt::AlignLeft | Qt::AlignVCenter, "FX");
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
    const QPoint pos = e->position().toPoint();

    if (e->button() == Qt::LeftButton) {
        const int tr = hitTransition(pos);
        if (tr > 0) {
            selectClip(tr);
            const int edge = 7;
            const QRect r = transitionRect(tr);
            if (std::abs(pos.x() - r.left()) <= edge || std::abs(pos.x() - r.right()) <= edge) {
                m_resizing = true;
                m_resizeIndex = tr;
                m_resizeStartX = pos.x();
                m_resizeStartDur = m_track->clips()[tr].transitionDur;
                emit transitionEditStarted(tr);
            } else {
                // jump to the middle of the blend so the preview shows it
                const double mid = m_track->clipStart(tr) + m_track->joinOverlap(tr) / 2.0;
                m_playhead = mid;
                emit seekRequested(mid);
            }
            update();
            return;
        }
        const int fx = hitEffect(pos);
        if (fx >= 0) {
            selectClip(fx);
            m_playhead = std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001));
            emit seekRequested(m_playhead);
            update();
            return;
        }
    }
    pick(pos, false);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = e->position().toPoint();

    if (m_resizing && (e->buttons() & Qt::LeftButton)) {
        auto& clips = m_track->clips();
        if (m_resizeIndex > 0 && m_resizeIndex < clips.size()) {
            const double maxDur = std::max(0.1, std::min(2.0, 0.5 * std::min(clips[m_resizeIndex - 1].duration(),
                                                                          clips[m_resizeIndex].duration())));
            const double nd = std::clamp(m_resizeStartDur + 2.0 * (pos.x() - m_resizeStartX) / m_pps, 0.1, maxDur);
            clips[m_resizeIndex].transitionDur = nd;
            update();
        }
        return;
    }
    if (e->buttons() & Qt::LeftButton) {
        pick(pos, true);
        return;
    }

    const int tr = hitTransition(pos);
    if (tr != m_hoverTrans) {
        m_hoverTrans = tr;
        update();
        if (tr > 0) {
            QToolTip::showText(e->globalPosition().toPoint(),
                QStringLiteral("%1  \u00B7  %2s\nDrag the edge to change the length")
                    .arg(transitionLabel(m_track->clips()[tr].transition))
                    .arg(m_track->clips()[tr].transitionDur, 0, 'f', 1), this);
        } else {
            QToolTip::hideText();
        }
    }
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (m_resizing) {
        m_resizing = false;
        m_resizeIndex = -1;
        emit transitionEditFinished();
        update();
        return;
    }
    QWidget::mouseReleaseEvent(e);
}

void TimelineWidget::leaveEvent(QEvent* e) {
    m_hoverTrans = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(e);
}

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    if (e->modifiers() & Qt::ControlModifier) {
        setPixelsPerSecond(m_pps * (e->angleDelta().y() > 0 ? 1.25 : 0.8));
        e->accept();
    } else QWidget::wheelEvent(e);
}

void TimelineWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasFormat(kEffectMime) || e->mimeData()->hasFormat(kTransMime)) {
        e->setDropAction(Qt::CopyAction);
        e->accept();
    } else e->ignore();
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent* e) {
    const QPoint pos = e->position().toPoint();
    if (e->mimeData()->hasFormat(kEffectMime)) {
        const int i = m_track->clipAt(std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001)));
        m_dragClip = (i >= 0) ? i : -1;
        m_dragJoin = -1;
        if (i >= 0) { e->setDropAction(Qt::CopyAction); e->accept(); } else e->ignore();
        update();
        return;
    }
    if (e->mimeData()->hasFormat(kTransMime)) {
        int j = nearestJoin(pos.x());
        if (j <= 0) {
            const int i = m_track->clipAt(std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001)));
            if (i > 0) j = i;
        }
        m_dragJoin = j;
        m_dragClip = -1;
        if (j > 0) { e->setDropAction(Qt::CopyAction); e->accept(); } else e->ignore();
        update();
        return;
    }
    e->ignore();
}

void TimelineWidget::dragLeaveEvent(QDragLeaveEvent* e) {
    m_dragClip = -1;
    m_dragJoin = -1;
    update();
    QWidget::dragLeaveEvent(e);
}

void TimelineWidget::dropEvent(QDropEvent* e) {
    const QPoint pos = e->position().toPoint();
    m_dragClip = -1;
    m_dragJoin = -1;

    if (e->mimeData()->hasFormat(kEffectMime)) {
        const QString name = QString::fromUtf8(e->mimeData()->data(kEffectMime));
        const int i = m_track->clipAt(std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001)));
        if (i >= 0 && !name.isEmpty()) {
            emit effectDropped(name, i);
            e->setDropAction(Qt::CopyAction);
            e->accept();
        } else e->ignore();
        update();
        return;
    }
    if (e->mimeData()->hasFormat(kTransMime)) {
        const QString name = QString::fromUtf8(e->mimeData()->data(kTransMime));
        int j = nearestJoin(pos.x());
        if (j <= 0) {
            const int i = m_track->clipAt(std::clamp(xToTime(pos.x()), 0.0, std::max(0.0, m_track->duration() - 0.001)));
            if (i > 0) j = i;
        }
        if (j > 0 && !name.isEmpty()) {
            emit transitionDropped(name, j);
            e->setDropAction(Qt::CopyAction);
            e->accept();
        } else e->ignore();
        update();
        return;
    }
    e->ignore();
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* e) {
    const int tr = hitTransition(e->pos());
    if (tr > 0) {
        QMenu menu(this);
        QAction* remove = menu.addAction("Remove transition");
        QAction* chosen = menu.exec(e->globalPos());
        if (chosen == remove) emit removeTransitionRequested(tr);
        return;
    }
    const int fx = hitEffect(e->pos());
    if (fx >= 0) {
        QMenu menu(this);
        QAction* remove = menu.addAction("Remove effect");
        QAction* chosen = menu.exec(e->globalPos());
        if (chosen == remove) emit removeEffectRequested(fx);
        return;
    }
    QWidget::contextMenuEvent(e);
}
