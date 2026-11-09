#include "timeline.h"
#include <algorithm>

double Track::duration() const {
    double total = 0;
    for (const Clip& c : m_clips) total += c.duration();
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
            m_clips[i] = left;
            m_clips.insert(i + 1, right);
            return true;
        }
        start = end;
    }
    return false;
}

bool Track::setSpeed(int index, double speed) {
    if (index < 0 || index >= m_clips.size()) return false;
    m_clips[index].speed = std::clamp(speed, 0.5, 2.0);
    return true;
}

void Track::remove(int index) {
    if (index >= 0 && index < m_clips.size()) m_clips.remove(index);
}
