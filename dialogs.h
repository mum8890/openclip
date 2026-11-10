#pragma once
#include <QWidget>
#include "timeline.h"
#include "templates.h"

bool editClipDialog(QWidget* parent, Clip& clip);
bool addTextDialog(QWidget* parent, TextItem& item);
bool exportDialog(QWidget* parent, ExportSettings& s, const QString& title = "Export");
bool templateDialog(QWidget* parent, VideoTemplate& chosen, QVector<SlotPick>& picks);
