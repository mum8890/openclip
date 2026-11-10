#include "exporter.h"
#include <QFile>
#include <algorithm>

namespace {

QString num(double v) { return QString::number(v, 'f', 3); }

QString fontPath(const QString& name) {
#ifdef Q_OS_WIN
    const QString dir = "C\\:/Windows/Fonts/";      // ':' must be escaped inside FFmpeg filters
    return dir + (name == "Impact" ? "impact.ttf" : "arialbd.ttf");
#else
    Q_UNUSED(name);
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
#endif
}

QString escapePath(QString p) {
    p.replace('\\', '/');
    p.replace(":", "\\:");
    return p;
}

// Per-clip colour / effect filters (each piece starts with a comma).
QString effectFilters(const Clip& c) {
    QString f;
    const QString& e = c.effect;
    if (e == "Black & White") f += ",hue=s=0";
    else if (e == "Vivid")     f += ",eq=saturation=1.5:contrast=1.1";
    else if (e == "Warm")      f += ",colorbalance=rs=0.15:gs=0.03:bs=-0.15";
    else if (e == "Cool")      f += ",colorbalance=rs=-0.12:bs=0.15";
    else if (e == "Cinematic") f += ",eq=contrast=1.15:saturation=0.9,vignette=PI/5";
    else if (e == "Sepia")     f += ",colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131";
    else if (e == "Blur")      f += ",gblur=sigma=6";
    else if (e == "Sharpen")   f += ",unsharp=5:5:1.2:5:5:0.0";
    if (c.brightness != 0.0 || c.contrast != 1.0 || c.saturation != 1.0)
        f += QString(",eq=brightness=%1:contrast=%2:saturation=%3")
                 .arg(num(c.brightness), num(c.contrast), num(c.saturation));
    return f;
}

} // namespace

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

QString Exporter::textFilter(const TextItem& t, int index, const ExportSettings& s) {
    struct Style { const char* font; const char* color; const char* border; double borderRel; bool box; };
    static const QHash<QString, Style> styles{
        {"Plain",          {"Arial",  "white",    "black",    0.0,  false}},
        {"Gaming",         {"Impact", "white",    "black",    0.07, false}},
        {"Yellow Outline", {"Impact", "0xFFD400", "black",    0.08, false}},
        {"Neon",           {"Arial",  "0x00F0FF", "0xFF00E5", 0.05, false}},
        {"Red Alert",      {"Impact", "0xFF2A2A", "white",    0.06, false}},
        {"Boxed",          {"Arial",  "white",    "black",    0.0,  true}},
    };
    const Style st = styles.value(t.style, styles["Gaming"]);

    // write the text to a file so we never have to escape special characters
    const QString file = m_tmp.path() + QString("/text%1.txt").arg(index);
    QFile f(file);
    if (f.open(QIODevice::WriteOnly)) { f.write(t.text.toUtf8()); f.close(); }

    const int fs = std::max(12, int(s.height() * t.sizePct / 100.0));
    const double S = t.start, E = t.start + t.duration;

    QString baseY = t.position == "Top"    ? "h*0.08"
                  : t.position == "Center" ? "(h-text_h)/2"
                                           : "h-text_h-h*0.08";
    QString y = baseY;
    if (t.animation == "Slide up")
        y = QString("%1+(1-min(1,(t-%2)/0.4))*h*0.05").arg(baseY, num(S));
    QString alpha = "1";
    if (t.animation == "Fade")
        alpha = QString("if(lt(t,%1+0.4),(t-%1)/0.4,if(gt(t,%2-0.4),(%2-t)/0.4,1))").arg(num(S), num(E));

    QString d = QString("drawtext=fontfile='%1':textfile='%2':fontsize=%3:fontcolor=%4")
                    .arg(fontPath(st.font), escapePath(file)).arg(fs).arg(st.color);
    if (st.borderRel > 0)
        d += QString(":borderw=%1:bordercolor=%2").arg(std::max(1, int(fs * st.borderRel))).arg(st.border);
    if (st.box)
        d += QString(":box=1:boxcolor=black@0.6:boxborderw=%1").arg(int(fs * 0.3));
    else
        d += ":shadowx=2:shadowy=2:shadowcolor=black@0.6";
    d += QString(":x='(w-text_w)/2':y='%1':alpha='%2':enable='between(t,%3,%4)'")
             .arg(y, alpha, num(S), num(E));
    return d;
}

QStringList Exporter::buildArgs(const Track& track, const QVector<TextItem>& texts,
                                const QString& output, const ExportSettings& s) {
    QStringList args{"-y", "-nostats", "-progress", "pipe:1"};
    QString g;
    const auto& clips = track.clips();
    const int n = clips.size();
    const int W = s.width(), H = s.height();

    const QString flags = (s.upscale == "None") ? "bicubic" : "lanczos";
    const QString pre  = s.upscale.contains("clean")   ? ",hqdn3d=1.5:1.5:6:6" : "";
    const QString post = s.upscale.contains("sharpen") ? ",unsharp=5:5:0.8:5:5:0.0" : "";
    const QString fit  = s.fill
        ? QString("scale=%1:%2:force_original_aspect_ratio=increase:flags=%3,crop=%1:%2").arg(W).arg(H).arg(flags)
        : QString("scale=%1:%2:force_original_aspect_ratio=decrease:flags=%3,"
                  "pad=%1:%2:(ow-iw)/2:(oh-ih)/2").arg(W).arg(H).arg(flags);

    for (int i = 0; i < n; ++i) {
        const Clip& c = clips[i];
        args << "-i" << c.path;
        g += QString("[%1:v]trim=start=%2:end=%3,setpts=(PTS-STARTPTS)/%4%5,%6,setsar=1,fps=%7%8%9,format=yuv420p[v%1];")
                 .arg(i).arg(num(c.sourceIn), num(c.sourceOut), num(c.speed), pre, fit)
                 .arg(s.fps).arg(post, effectFilters(c));
        if (c.hasAudio)
            g += QString("[%1:a]atrim=start=%2:end=%3,asetpts=PTS-STARTPTS,atempo=%4,aresample=48000[a%1];")
                     .arg(i).arg(num(c.sourceIn), num(c.sourceOut), num(c.speed));
        else
            g += QString("anullsrc=r=48000:cl=stereo,atrim=duration=%2,asetpts=PTS-STARTPTS[a%1];")
                     .arg(i).arg(num(c.duration()));
    }

    if (!track.hasTransitions()) {
        for (int i = 0; i < n; ++i) g += QString("[v%1][a%1]").arg(i);
        g += QString("concat=n=%1:v=1:a=1[vx][a];").arg(n);
    } else {
        QString vcur = "v0", acur = "a0";
        double acc = clips[0].duration();
        for (int i = 1; i < n; ++i) {
            const double ov = track.joinOverlap(i);
            const QString tr = clips[i].transition == "None" ? "fade" : clips[i].transition;
            const QString vout = (i == n - 1) ? "vx" : QString("xv%1").arg(i);
            const QString aout = (i == n - 1) ? "a"  : QString("xa%1").arg(i);
            g += QString("[%1][v%2]xfade=transition=%3:duration=%4:offset=%5[%6];")
                     .arg(vcur).arg(i).arg(tr, num(ov), num(acc - ov), vout);
            g += QString("[%1][a%2]acrossfade=d=%3:c1=tri:c2=tri[%4];")
                     .arg(acur).arg(i).arg(num(ov), aout);
            acc += clips[i].duration() - ov;
            vcur = vout; acur = aout;
        }
    }

    // text overlays on the finished video
    QStringList dts;
    for (int i = 0; i < texts.size(); ++i) dts << textFilter(texts[i], i, s);
    g += "[vx]" + (dts.isEmpty() ? QString("null") : dts.join(",")) + "[v]";

    const bool hevc = s.base >= 4320;     // 8K: use H.265, H.264 cannot do it reliably
    args << "-filter_complex" << g << "-map" << "[v]" << "-map" << "[a]"
         << "-c:v" << (hevc ? "libx265" : "libx264") << "-preset" << s.preset
         << "-crf" << QString::number(s.crf);
    if (hevc) args << "-tag:v" << "hvc1";
    args << "-pix_fmt" << "yuv420p" << "-c:a" << "aac" << "-b:a" << "192k"
         << "-movflags" << "+faststart" << output;
    return args;
}

void Exporter::start(const Track& track, const QVector<TextItem>& texts,
                     const QString& output, const ExportSettings& s) {
    if (track.clips().isEmpty()) { emit finished(false, "Timeline is empty."); return; }
    m_total = track.exportDuration();
    m_log.clear();
    m_proc.start("ffmpeg", buildArgs(track, texts, output, s));
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
