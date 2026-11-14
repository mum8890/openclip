#include "project.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

static QString pick(const QString& v, const QStringList& allowed, const QString& fallback) {
    return allowed.contains(v) ? v : fallback;
}

QByteArray projectToJson(const ProjectData& p) {
    QJsonArray clips;
    for (const Clip& c : p.clips) {
        clips.append(QJsonObject{
            {"path", c.path}, {"sourceIn", c.sourceIn}, {"sourceOut", c.sourceOut}, {"speed", c.speed},
            {"hasAudio", c.hasAudio}, {"effect", c.effect}, {"brightness", c.brightness},
            {"contrast", c.contrast}, {"saturation", c.saturation},
            {"transition", c.transition}, {"transitionDur", c.transitionDur}});
    }
    QJsonArray texts;
    for (const TextItem& t : p.texts) {
        texts.append(QJsonObject{
            {"text", t.text}, {"start", t.start}, {"duration", t.duration}, {"style", t.style},
            {"position", t.position}, {"animation", t.animation}, {"size", t.sizePct}});
    }
    QJsonArray media;
    for (const MediaEntry& m : p.media)
        media.append(QJsonObject{{"path", m.path}, {"duration", m.duration}, {"hasAudio", m.hasAudio}});
    const ExportSettings& s = p.settings;
    QJsonObject root{
        {"app", "OpenClip"}, {"format", 1},
        {"clips", clips}, {"texts", texts}, {"media", media},
        {"export", QJsonObject{{"base", s.base}, {"aspect", s.aspect}, {"fps", s.fps}, {"crf", s.crf},
                               {"fill", s.fill}, {"upscale", s.upscale}, {"preset", s.preset}}}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool projectFromJson(const QByteArray& json, ProjectData& out, QString* error) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) { if (error) *error = "This file is not a valid project."; return false; }
    const QJsonObject root = doc.object();
    if (root.value("app").toString() != "OpenClip") { if (error) *error = "This file was not made by OpenClip."; return false; }
    if (root.value("format").toInt(1) > 1) { if (error) *error = "This project was saved by a newer version of OpenClip. Please update first."; return false; }

    out = ProjectData{};
    for (const QJsonValue& v : root.value("clips").toArray()) {
        const QJsonObject o = v.toObject();
        Clip c;
        c.path = o.value("path").toString();
        c.sourceIn = std::max(0.0, o.value("sourceIn").toDouble());
        c.sourceOut = o.value("sourceOut").toDouble();
        if (c.path.isEmpty() || c.sourceOut - c.sourceIn <= 0.01) continue;
        c.speed = std::clamp(o.value("speed").toDouble(1.0), 0.5, 2.0);
        c.hasAudio = o.value("hasAudio").toBool(true);
        c.effect = pick(o.value("effect").toString("None"), kEffects, "None");
        c.brightness = std::clamp(o.value("brightness").toDouble(0.0), -1.0, 1.0);
        c.contrast = std::clamp(o.value("contrast").toDouble(1.0), 0.0, 3.0);
        c.saturation = std::clamp(o.value("saturation").toDouble(1.0), 0.0, 3.0);
        c.transition = pick(o.value("transition").toString("None"), kTransitions, "None");
        c.transitionDur = std::clamp(o.value("transitionDur").toDouble(0.5), 0.1, 3.0);
        out.clips.append(c);
    }
    for (const QJsonValue& v : root.value("texts").toArray()) {
        const QJsonObject o = v.toObject();
        TextItem t;
        t.text = o.value("text").toString();
        if (t.text.isEmpty()) continue;
        t.start = std::max(0.0, o.value("start").toDouble());
        t.duration = std::clamp(o.value("duration").toDouble(3.0), 0.5, 600.0);
        t.style = pick(o.value("style").toString(), kTextStyles, "Gaming");
        t.position = pick(o.value("position").toString(), kTextPositions, "Bottom");
        t.animation = pick(o.value("animation").toString(), kTextAnimations, "Fade");
        t.sizePct = std::clamp(o.value("size").toInt(9), 3, 30);
        out.texts.append(t);
    }
    for (const QJsonValue& v : root.value("media").toArray()) {
        const QJsonObject o = v.toObject();
        MediaEntry m{o.value("path").toString(), o.value("duration").toDouble(), o.value("hasAudio").toBool(true)};
        if (!m.path.isEmpty()) out.media.append(m);
    }
    const QJsonObject e = root.value("export").toObject();
    if (!e.isEmpty()) {
        out.settings.base = std::clamp(e.value("base").toInt(1080), 480, 4320);
        out.settings.aspect = pick(e.value("aspect").toString("16:9"), {"16:9", "9:16", "1:1"}, "16:9");
        out.settings.fps = std::clamp(e.value("fps").toInt(60), 24, 60);
        out.settings.crf = std::clamp(e.value("crf").toInt(18), 10, 40);
        out.settings.fill = e.value("fill").toBool(false);
        out.settings.upscale = pick(e.value("upscale").toString("Lanczos + sharpen"),
            {"None", "Lanczos", "Lanczos + sharpen", "Lanczos + clean + sharpen"}, "Lanczos + sharpen");
        out.settings.preset = pick(e.value("preset").toString("medium"), {"veryfast", "medium", "slow"}, "medium");
    }
    return true;
}
