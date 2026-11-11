#pragma once
#include "timeline.h"

struct MediaEntry { QString path; double duration = 0; bool hasAudio = true; };

struct ProjectData {
    QVector<Clip> clips;
    QVector<TextItem> texts;
    ExportSettings settings;
    QVector<MediaEntry> media;
};

QByteArray projectToJson(const ProjectData& p);
// Returns false and fills `error` if the file is not a valid OpenClip project.
bool projectFromJson(const QByteArray& json, ProjectData& out, QString* error = nullptr);
