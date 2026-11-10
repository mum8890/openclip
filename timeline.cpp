#include "timeline.h"
#include <algorithm>

double Track::duration() const {
    double total = 0;
    for (const Clip& c : m_clips) total += c.duration();
    return total;
}

bool Track::hasTransitions() const {
    for (int i = 1; i < m_clips.size(); ++i)
        if (m_clips[i].transition != "None") return true;
    return false;
}

double Track::joinOverlap(int i) const {
    if (i <= 0 || i >= m_clips.size() || !hasTransitions()) return 0;
    const Clip& c = m_clips[i];
    const double maxO = 0.5 * std::min(m_clips[i - 1].duration(), c.duration());
    const double want = (c.transition == "None") ? 0.04 : c.transitionDur;   // tiny overlap = a plain cut
    return std::clamp(want, std::min(0.04, maxO), maxO);
}

double Track::exportDuration() const {
    double total = duration();
    for (int i = 1; i < m_clips.size(); ++i) total -= joinOverlap(i);
    return total;
}

double Track::clipStart(int index) const {
    double s = 0;
    for (int i = 0; i < index && i < m_clips.size(); ++i) s += m_clips[i].duration();
    return s;
}

int Track::clipAt(double t) const {
    if (m_clips.isEmpty()) return -1;
    double start = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        const double end = start + m_clips[i].duration();
        if (t < end) return i;
        start = end;
    }
    return m_clips.size() - 1;
}

bool Track::splitAt(double t) {
    double start = 0;
    for (int i = 0; i < m_clips.size(); ++i) {
        const double end = start + m_clips[i].duration();
        constexpr double eps = 0.05;
        if (t > start + eps && t < end - eps) {
            Clip left = m_clips[i];
            Clip right = m_clips[i];
            const double cut = left.sourceIn + (t - start) * left.speed;
            left.sourceOut = cut;
            right.sourceIn = cut;
            right.transition = "None";      // the new join is a plain cut
            m_clips[i] = left;
            m_clips.insert(i + 1, right);
            return true;
        }
        start = end;
    }
    return false;
}

void Track::remove(int index) {
    if (index >= 0 && index < m_clips.size()) m_clips.remove(index);
}
