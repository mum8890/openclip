#include "timeline.h"
#include <QHash>
#include <algorithm>

// The timeline and the exported video share one clock: transitions overlap the clips,
// so they shorten the project instead of shifting everything after them.
double Track::duration() const { return exportDuration(); }

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
    double total = 0;
    for (const Clip& c : m_clips) total += c.duration();
    for (int i = 1; i < m_clips.size(); ++i) total -= joinOverlap(i);
    return std::max(0.0, total);
}

// Start of a clip on the timeline / in the finished video. A transition starts at this
// moment and overlaps the tail of the previous clip, exactly like FFmpeg's xfade does.
double Track::clipStart(int index) const {
    double s = 0;
    for (int i = 0; i < index && i < m_clips.size(); ++i)
        s += m_clips[i].duration() - joinOverlap(i + 1);
    return s;
}

int Track::clipAt(double t) const {
    if (m_clips.isEmpty()) return -1;
    // Walk backwards: inside a transition both clips exist, and the incoming one is on top.
    for (int i = m_clips.size() - 1; i >= 0; --i)
        if (t >= clipStart(i)) return i;
    return 0;
}

bool Track::splitAt(double t) {
    for (int i = 0; i < m_clips.size(); ++i) {
        const double start = clipStart(i);
        const double end = start + m_clips[i].duration();
        const double inOverlap = (i > 0) ? joinOverlap(i) : 0.0;
        const double outOverlap = (i + 1 < m_clips.size()) ? joinOverlap(i + 1) : 0.0;
        constexpr double eps = 0.05;
        // never cut inside a transition: half a dissolve would look broken
        if (t > start + inOverlap + eps && t < end - outOverlap - eps) {
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
    }
    return false;
}

void Track::remove(int index) {
    if (index >= 0 && index < m_clips.size()) m_clips.remove(index);
}

QString transitionLabel(const QString& name) {
    static const QHash<QString, QString> labels{
        {"None", "None (plain cut)"},
        {"fade", "Fade"}, {"fadeblack", "Fade through black"}, {"fadewhite", "Fade through white"},
        {"fadegrays", "Fade through grey"}, {"dissolve", "Dissolve"},
        {"wipeleft", "Wipe left"}, {"wiperight", "Wipe right"}, {"wipeup", "Wipe up"}, {"wipedown", "Wipe down"},
        {"wipetl", "Wipe to top-left"}, {"wipetr", "Wipe to top-right"},
        {"wipebl", "Wipe to bottom-left"}, {"wipebr", "Wipe to bottom-right"},
        {"slideleft", "Slide left"}, {"slideright", "Slide right"},
        {"slideup", "Slide up"}, {"slidedown", "Slide down"},
        {"smoothleft", "Smooth left"}, {"smoothright", "Smooth right"},
        {"smoothup", "Smooth up"}, {"smoothdown", "Smooth down"},
        {"circleopen", "Circle open"}, {"circleclose", "Circle close"},
        {"circlecrop", "Circle crop"}, {"rectcrop", "Rectangle crop"},
        {"vertopen", "Vertical open"}, {"vertclose", "Vertical close"},
        {"horzopen", "Horizontal open"}, {"horzclose", "Horizontal close"},
        {"diagtl", "Diagonal top-left"}, {"diagtr", "Diagonal top-right"},
        {"diagbl", "Diagonal bottom-left"}, {"diagbr", "Diagonal bottom-right"},
        {"hlslice", "Horizontal slices"}, {"hrslice", "Reverse horizontal slices"},
        {"vuslice", "Vertical slices"}, {"vdslice", "Reverse vertical slices"},
        {"pixelize", "Pixelate"}, {"radial", "Radial wipe"},
        {"zoomin", "Zoom in"}, {"hblur", "Blur"},
        {"squeezeh", "Squeeze horizontal"}, {"squeezev", "Squeeze vertical"},
        {"coverleft", "Cover left"}, {"coverright", "Cover right"},
        {"coverup", "Cover up"}, {"coverdown", "Cover down"},
        {"revealleft", "Reveal left"}, {"revealright", "Reveal right"},
        {"revealup", "Reveal up"}, {"revealdown", "Reveal down"},
    };
    return labels.value(name, name);
}
