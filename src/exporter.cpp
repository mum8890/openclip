#include "exporter.h"

Exporter::Exporter(QObject* parent) : QObject(parent) {
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_proc, &QProcess::readyRead, this, &Exporter::onReadyRead);
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        emit finished(false, "Could not run ffmpeg. Is it installed and on PATH?");
    });
    connect(&m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st) {
                emit finished(code == 0 && st == QProcess::NormalExit, m_log);
            });
}

QStringList Exporter::buildArgs(const Track& track, const QString& output,
                                int w, int h, int fps) {
    QStringList args{"-y", "-nostats", "-progress", "pipe:1"};
    QString graph;
    const auto& clips = track.clips();
    const int n = clips.size();

    for (int i = 0; i < n; ++i) {
        const Clip& c = clips[i];
        args << "-i" << c.path;
        // video: cut, apply speed, fit to canvas, fixed frame rate
        graph += QString(
            "[%1:v]trim=start=%2:end=%3,setpts=(PTS-STARTPTS)/%7,"
            "scale=%4:%5:force_original_aspect_ratio=decrease,"
            "pad=%4:%5:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=%6[v%1];")
            .arg(i).arg(c.sourceIn, 0, 'f', 3).arg(c.sourceOut, 0, 'f', 3)
            .arg(w).arg(h).arg(fps).arg(c.speed, 0, 'f', 3);
        // audio: real audio if present, otherwise silence of the right length
        if (c.hasAudio) {
            graph += QString("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,atempo=%4,aresample=48000[a%1];")
                .arg(i).arg(c.sourceIn, 0, 'f', 3).arg(c.sourceOut, 0, 'f', 3).arg(c.speed, 0, 'f', 3);
        } else {
            graph += QString("anullsrc=r=48000:cl=stereo,atrim=duration=%2,asetpts=PTS-STARTPTS[a%1];")
                .arg(i).arg(c.duration(), 0, 'f', 3);
        }
    }
    for (int i = 0; i < n; ++i) graph += QString("[v%1][a%1]").arg(i);
    graph += QString("concat=n=%1:v=1:a=1[v][a]").arg(n);

    args << "-filter_complex" << graph
         << "-map" << "[v]" << "-map" << "[a]"
         << "-c:v" << "libx264" << "-preset" << "medium" << "-crf" << "18"
         << "-pix_fmt" << "yuv420p" << "-c:a" << "aac" << "-b:a" << "192k"
         << output;
    return args;
}

void Exporter::start(const Track& track, const QString& output, int fps) {
    if (track.clips().isEmpty()) { emit finished(false, "Timeline is empty."); return; }
    m_total = track.duration();
    m_log.clear();
    m_proc.start("ffmpeg", buildArgs(track, output, 1920, 1080, fps));
}

void Exporter::onReadyRead() {
    const QString text = QString::fromUtf8(m_proc.readAll());
    m_log += text;
    for (const QString& line : text.split('\n')) {
        if (line.startsWith("out_time_us=") && m_total > 0) {
            bool ok = false;
            const double us = line.mid(12).trimmed().toDouble(&ok);
            if (ok) emit progress(qBound(0.0, us / 1e6 / m_total, 1.0));
        }
    }
}
