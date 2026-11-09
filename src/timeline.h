#pragma once
#include <QString>
#include <QVector>

// A piece of a source file placed on a track. Times are in seconds.
struct Clip {
    QString path;
    double sourceIn  = 0.0;
    double sourceOut = 0.0;
    double speed     = 1.0;     // 0.5 = slow-mo, 2.0 = fast-forward (range 0.5-2.0)
    bool   hasAudio  = true;
    double duration() const { return (sourceOut - sourceIn) / speed; }   // time on the timeline
};

// Clips on a track play back-to-back (no gaps yet).
class Track {
public:
    const QVector<Clip>& clips() const { return m_clips; }
    QVector<Clip>& clips() { return m_clips; }

    double duration() const;
    double clipStart(int index) const;              // timeline time where clip `index` begins
    int    clipAt(double t) const;                  // index of clip covering time t (-1 if empty)
    bool   splitAt(double t);
    bool   setSpeed(int index, double speed);
    void   remove(int index);

private:
    QVector<Clip> m_clips;
};

class Timeline {
public:
    Timeline() { m_tracks.append(Track{}); }   // one track for now; multi-track later
    Track& mainTrack() { return m_tracks[0]; }
    const Track& mainTrack() const { return m_tracks[0]; }
private:
    QVector<Track> m_tracks;
};
