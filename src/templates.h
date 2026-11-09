#pragma once
#include <functional>
#include "timeline.h"

// A template = a ready-made edit with empty slots. The user only picks a video for each slot.
struct TemplateSlot {
    QString label;
    double duration = 3.0;          // seconds on the timeline
    double speed = 1.0;
    QString effect = "None";
    QString transition = "None";    // into this slot
    double transitionDur = 0.5;
};

struct VideoTemplate {
    QString name;
    QString description;
    QVector<TemplateSlot> parts;
    QVector<TextItem> texts;
    ExportSettings settings;
};

struct SlotPick { QString path; double startAt = 0.0; };

bool parseTemplate(const QByteArray& json, VideoTemplate& out);
bool loadTemplateFile(const QString& path, VideoTemplate& out);
QVector<VideoTemplate> loadBuiltInTemplates();

using ProbeFn = std::function<bool(const QString& path, double& duration, bool& hasAudio)>;
// Fills the track/texts/settings from the template. Returns an error message, empty on success.
QString applyTemplate(const VideoTemplate& t, const QVector<SlotPick>& picks, Track& track,
                      QVector<TextItem>& texts, ExportSettings& settings, const ProbeFn& probe);
