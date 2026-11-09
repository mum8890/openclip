#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

inline const QStringList kEffects{"None", "Black & White", "Vivid", "Warm", "Cool",
                                  "Cinematic", "Sepia", "Blur", "Sharpen"};
// names are FFmpeg xfade transitions
inline const QStringList kTransitions{"None", "fade", "fadeblack", "fadewhite", "dissolve",
                                      "wipeleft", "wiperight", "slideleft", "slideright",
                                      "circleopen", "circleclose", "pixelize", "radial",
                                      "zoomin", "hblur"};
inline const QStringList kTextStyles{"Gaming", "Yellow Outline", "Neon", "Red Alert", "Boxed", "Plain"};
inline const QStringList kTextPositions{"Top", "Center", "Bottom"};
inline const QStringList kTextAnimations{"Fade", "Slide up", "None"};

// A piece of a source file placed on a track. Times are in seconds.
struct Clip {
    QString path;
    double sourceIn  = 0.0;
    double sourceOut = 0.0;
    double speed     = 1.0;     // 0.5 = slow-mo, 2.0 = fast-forward
    bool   hasAudio  = true;
    // look
    QString effect = "None";
    double brightness = 0.0;    // -1..1
    double contrast   = 1.0;    // 0..3
    double saturation = 1.0;    // 0..3
    // transition INTO this clip from the previous one (export only)
    QString transition = "None";
    double transitionDur = 0.5;
    double duration() const { return (sourceOut - sourceIn) / speed; }
};

struct TextItem {
    QString text = "Your text";
    double start = 0.0;          // seconds in the exported video
    double duration = 3.0;
    QString style = "Gaming";
    QString position = "Bottom";
    QString animation = "Fade";
    int sizePct = 9;             // font height as % of video height
};

struct ExportSettings {
    int base = 1080;             // short side: 480, 720, 1080, 1440, 2160 (4K), 4320 (8K)
    QString aspect = "16:9";     // "16:9", "9:16", "1:1"
    int fps = 60;
    int crf = 18;                // lower = better quality, bigger file
    bool fill = false;           // true = crop to fill frame, false = keep whole picture with bars
    QString upscale = "Lanczos + sharpen";   // None / Lanczos / Lanczos + sharpen / Lanczos + clean + sharpen
    QString preset = "medium";   // x264/x265 speed preset

    static int even(int v) { return (v + 1) / 2 * 2; }
    int width() const {
        if (aspect == "9:16" || aspect == "1:1") return even(base);
        return even(base * 16 / 9);
    }
    int height() const {
        if (aspect == "9:16") return even(base * 16 / 9);
        return even(base);
    }
};

class Track {
public:
    const QVector<Clip>& clips() const { return m_clips; }
    QVector<Clip>& clips() { return m_clips; }

    double duration() const;                 // length as shown on the timeline
    double exportDuration() const;           // length after transition overlaps
    bool   hasTransitions() const;
    double joinOverlap(int index) const;     // seconds clip `index` overlaps the previous one in the export
    double clipStart(int index) const;
    int    clipAt(double t) const;
    bool   splitAt(double t);
    void   remove(int index);

private:
    QVector<Clip> m_clips;
};

class Timeline {
public:
    Timeline() { m_tracks.append(Track{}); }
    Track& mainTrack() { return m_tracks[0]; }
    const Track& mainTrack() const { return m_tracks[0]; }
private:
    QVector<Track> m_tracks;
};
