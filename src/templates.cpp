#include "templates.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

static QString pick(const QString& v, const QStringList& allowed, const QString& fallback) {
    return allowed.contains(v) ? v : fallback;
}

bool parseTemplate(const QByteArray& json, VideoTemplate& out) {
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    out = VideoTemplate{};
    out.name = o.value("name").toString();
    out.description = o.value("description").toString();

    for (const QJsonValue& v : o.value("slots").toArray()) {
        const QJsonObject s = v.toObject();
        TemplateSlot slot;
        slot.label = s.value("label").toString("Clip");
        slot.duration = std::clamp(s.value("duration").toDouble(3.0), 0.5, 600.0);
        slot.speed = std::clamp(s.value("speed").toDouble(1.0), 0.5, 2.0);
        slot.effect = pick(s.value("effect").toString("None"), kEffects, "None");
        slot.transition = pick(s.value("transition").toString("None"), kTransitions, "None");
        slot.transitionDur = std::clamp(s.value("transitionDur").toDouble(0.5), 0.1, 3.0);
        out.parts.append(slot);
    }
    for (const QJsonValue& v : o.value("texts").toArray()) {
        const QJsonObject s = v.toObject();
        TextItem t;
        t.text = s.value("text").toString();
        if (t.text.isEmpty()) continue;
        t.start = std::max(0.0, s.value("start").toDouble(0));
        t.duration = std::clamp(s.value("duration").toDouble(3), 0.5, 600.0);
        t.style = pick(s.value("style").toString(), kTextStyles, "Gaming");
        t.position = pick(s.value("position").toString(), kTextPositions, "Bottom");
        t.animation = pick(s.value("animation").toString(), kTextAnimations, "Fade");
        t.sizePct = std::clamp(s.value("size").toInt(9), 3, 30);
        out.texts.append(t);
    }
    const QJsonObject e = o.value("export").toObject();
    if (!e.isEmpty()) {
        out.settings.aspect = pick(e.value("aspect").toString("16:9"), {"16:9", "9:16", "1:1"}, "16:9");
        out.settings.fps = std::clamp(e.value("fps").toInt(60), 24, 60);
        out.settings.base = std::clamp(e.value("base").toInt(1080), 480, 4320);
        out.settings.fill = e.value("fill").toBool(false);
    }
    return !out.name.isEmpty() && !out.parts.isEmpty();
}

bool loadTemplateFile(const QString& path, VideoTemplate& out) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    return parseTemplate(f.readAll(), out);
}

QVector<VideoTemplate> loadBuiltInTemplates() {
    QVector<VideoTemplate> all;
    const QDir dir(":/templates");
    for (const QString& name : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) {
        VideoTemplate t;
        if (loadTemplateFile(dir.filePath(name), t)) all.append(t);
    }
    return all;
}

QString applyTemplate(const VideoTemplate& t, const QVector<SlotPick>& picks, Track& track,
                      QVector<TextItem>& texts, ExportSettings& settings, const ProbeFn& probe) {
    if (picks.size() != t.parts.size()) return "Please choose a video for every part of the template.";
    QVector<Clip> clips;
    for (int i = 0; i < t.parts.size(); ++i) {
        const TemplateSlot& s = t.parts[i];
        if (picks[i].path.isEmpty()) return QString("Choose a video for \"%1\".").arg(s.label);
        double dur = 0; bool audio = true;
        if (!probe(picks[i].path, dur, audio) || dur <= 0)
            return QString("Could not read the video for \"%1\" (is ffprobe installed?).").arg(s.label);
        Clip c;
        c.path = picks[i].path;
        c.sourceIn = std::clamp(picks[i].startAt, 0.0, std::max(0.0, dur - 0.3));
        c.sourceOut = std::min(dur, c.sourceIn + s.duration * s.speed);
        if (c.sourceOut - c.sourceIn < 0.3)
            return QString("The video for \"%1\" is too short from that start time.").arg(s.label);
        c.speed = s.speed; c.hasAudio = audio; c.effect = s.effect;
        c.transition = (i == 0) ? "None" : s.transition;
        c.transitionDur = s.transitionDur;
        clips.append(c);
    }
    track.clips() = clips;
    texts = t.texts;
    settings = t.settings;
    return {};
}
