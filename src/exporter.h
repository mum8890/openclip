#pragma once
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include "timeline.h"

// Renders a Track (+ text overlays) with one FFmpeg filter_complex graph, and can also
// draw a single still frame of the finished result for the live preview.
class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject* parent = nullptr);
    ~Exporter() override;

    QStringList buildArgs(const Track& track, const QVector<TextItem>& texts,
                          const QString& output, const ExportSettings& s);

    // One frame of the finished video at `atTime`: clip look, transition blend and titles.
    // Returns an empty list when there is nothing to draw.
    QStringList buildStillArgs(const Track& track, const QVector<TextItem>& texts,
                               double atTime, const ExportSettings& s, const QString& outputPng,
                               int previewWidth = 960);

    void start(const Track& track, const QVector<TextItem>& texts,
               const QString& output, const ExportSettings& s);
    void cancel();
    bool isRunning() const { return m_proc.state() != QProcess::NotRunning; }

    // Small + fast settings for the live/FX preview (same shape as the real export).
    static ExportSettings previewSettings(const ExportSettings& s);

signals:
    void progress(double fraction);
    void finished(bool ok, const QString& log);

private:
    void onReadyRead();
    QString textFilter(const TextItem& t, int index, int videoHeight, bool still);
    QString m_log;
    QString m_lineBuffer;
    QProcess m_proc;
    QTemporaryDir m_tmp;
    double m_total = 0;
    QString m_output;
    bool m_cancelled = false;
};
