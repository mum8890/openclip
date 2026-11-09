#pragma once
#include <QObject>
#include <QProcess>
#include "timeline.h"

// Renders a Track to a file by building one FFmpeg filter_complex graph.
// Requires `ffmpeg` on PATH.
class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject* parent = nullptr);

    static QStringList buildArgs(const Track& track, const QString& output,
                                 int width, int height, int fps);
    void start(const Track& track, const QString& output, int fps = 60);

signals:
    void progress(double fraction);          // 0..1
    void finished(bool ok, const QString& log);

private:
    void onReadyRead();
    QProcess m_proc;
    double m_total = 0;
    QString m_log;
};
