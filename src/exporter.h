#pragma once
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include "timeline.h"

// Renders a Track (+ text overlays) with one FFmpeg filter_complex graph.
class Exporter : public QObject {
    Q_OBJECT
public:
    explicit Exporter(QObject* parent = nullptr);

    QStringList buildArgs(const Track& track, const QVector<TextItem>& texts,
                          const QString& output, const ExportSettings& s);
    void start(const Track& track, const QVector<TextItem>& texts,
               const QString& output, const ExportSettings& s);

signals:
    void progress(double fraction);
    void finished(bool ok, const QString& log);

private:
    void onReadyRead();
    QString textFilter(const TextItem& t, int index, const ExportSettings& s);
    QProcess m_proc;
    QTemporaryDir m_tmp;
    double m_total = 0;
    QString m_log;
};
