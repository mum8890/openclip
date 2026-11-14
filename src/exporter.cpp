#include "exporter.h"
#include <QFile>
#include <QHash>
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
    if (e == "Black & White")  f += ",hue=s=0";
    else if (e == "Vivid")     f += ",eq=saturation=1.5:contrast=1.1";
    else if (e == "Warm")      f += ",colorbalance=rs=0.15:gs=0.03:bs=-0.15";
    else if (e == "Cool")      f += ",colorbalance=rs=-0.12:bs=0.15";
    else if (e == "Cinematic") f += ",eq=contrast=1.15:saturation=0.9,vignette=PI/5";
    else if (e == "Sepia")     f += ",colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131";
    else if (e == "Blur")      f += ",gblur=sigma=6";
    else if (e == "Sharpen")   f += ",unsharp=5:5:1.2:5:5:0.0";
    else if (e == "Noir")      f += ",hue=s=0,eq=contrast=1.35:brightness=-0.04,curves=preset=medium_contrast";
    else if (e == "Golden Hour") f += ",colorbalance=rs=0.22:gs=0.10:bs=-0.22,eq=saturation=1.15:brightness=0.02";
    else if (e == "Teal & Orange") f += ",colorbalance=rs=0.18:bs=0.22:gm=0.02,eq=saturation=1.2:contrast=1.08";
    else if (e == "Cyberpunk") f += ",colorbalance=rs=0.22:gs=-0.02:bs=0.28,eq=saturation=1.4:contrast=1.12";
    else if (e == "Cold Night") f += ",colorbalance=rs=-0.15:bs=0.28,eq=brightness=-0.04:contrast=1.12:saturation=0.95";
    else if (e == "Vintage")   f += ",curves=preset=vintage,eq=saturation=0.9";
    else if (e == "Faded Film") f += ",curves=preset=lighter,eq=saturation=0.75:contrast=0.92";
    else if (e == "Punch")     f += ",eq=saturation=1.35:contrast=1.18,unsharp=5:5:0.9:5:5:0";
    else if (e == "Dream")     f += ",gblur=sigma=2.5,eq=brightness=0.08:saturation=1.2:contrast=0.95";
    else if (e == "Old Film")  f += ",curves=preset=vintage,noise=alls=10:allf=t+u,vignette=PI/4";
    else if (e == "Invert")    f += ",negate";
    else if (e == "Mirror")    f += ",hflip";
    else if (e == "Sketch")    f += ",edgedetect=mode=colormix:high=0.4:low=0.1,eq=contrast=1.2";
    else if (e == "Infrared")  f += ",hue=h=180:s=1.6";
    else if (e == "Killcam Red") f += ",colorbalance=rs=0.30:gs=-0.10:bs=-0.15,eq=saturation=1.3:contrast=1.15";
    else if (e == "Night Vision") f += ",hue=s=0,colorbalance=gs=0.5:rs=-0.2:bs=-0.2,eq=contrast=1.2,noise=alls=6:allf=t";
    else if (e == "HDR Boost") f += ",eq=contrast=1.12:saturation=1.25:gamma=1.05,unsharp=5:5:0.6";
    if (c.brightness != 0.0 || c.contrast != 1.0 || c.saturation != 1.0)
        f += QString(",eq=brightness=%1:contrast=%2:saturation=%3")
                 .arg(num(c.brightness), num(c.contrast), num(c.saturation));
    return f;
}

// Scale a frame to the output size (or keep it whole with bars).
QString fitFilter(int w, int h, bool fill, const QString& flags) {
    return fill
        ? QString("scale=%1:%2:force_original_aspect_ratio=increase:flags=%3,crop=%1:%2").arg(w).arg(h).arg(flags)
        : QString("scale=%1:%2:force_original_aspect_ratio=decrease:flags=%3,"
                  "pad=%1:%2:(ow-iw)/2:(oh-ih)/2").arg(w).arg(h).arg(flags);
}

} // namespace

Exporter::Exporter(QObject* parent) : QObject(parent) {
    m_proc.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_proc, &QProcess::readyRead, this, &Exporter::onReadyRead);
    connect(&m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_cancelled) return;
        emit finished(false, "Could not run ffmpeg. Is it installed and on PATH?");
    });
    connect(&m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus st) {
                const bool cancelled = m_cancelled;
                m_cancelled = false;
                if (cancelled) {
                    if (!m_output.isEmpty()) QFile::remove(m_output);
                    emit finished(false, "Cancelled.");
                    return;
                }
                emit finished(code == 0 && st == QProcess::NormalExit, m_log);
            });
}

Exporter::~Exporter() {
    if (m_proc.state() != QProcess::NotRunning) {
        m_cancelled = true;
        m_proc.kill();
        m_proc.waitForFinished(2000);
    }
}

ExportSettings Exporter::previewSettings(const ExportSettings& s) {
    ExportSettings p;
    p.base = 480; p.fps = 30; p.crf = 30; p.preset = "ultrafast"; p.upscale = "None";
    p.aspect = s.aspect; p.fill = s.fill;
    return p;
}

QString Exporter::textFilter(const TextItem& t, int index, int videoHeight, bool still) {
    struct Style { const char* font; const char* color; const char* border; double borderRel; bool box; };
    static const QHash<QString, Style> styles{
        {"Plain",          {"Arial",  "white",    "black",    0.0,  false}},
        {"Gaming",         {"Impact", "white",    "black",    0.07, false}},
        {"Yellow Outline", {"Impact", "0xFFD400", "black",    0.08, false}},
        {"Neon",           {"Arial",  "0x00F0FF", "0xFF00E5", 0.05, false}},
        {"Red Alert",      {"Impact", "0xFF2A2A", "white",    0.06, false}},
        {"Boxed",          {"Arial",  "white",    "black",    0.0,  true}},
    };
    const Style fallback{"Impact", "white", "black", 0.07, false};
    const auto found = styles.constFind(t.style);
    const Style st = (found == styles.constEnd()) ? fallback : found.value();

    // write the text to a file so we never have to escape special characters
    const QString file = m_tmp.path() + QString("/text%1.txt").arg(index);
    QFile f(file);
    if (f.open(QIODevice::WriteOnly)) { f.write(t.text.toUtf8()); f.close(); }

    const int fs = std::max(12, int(videoHeight * t.sizePct / 100.0));
    const double S = t.start, E = t.start + t.duration;

    QString baseY = t.position == "Top"    ? "h*0.08"
                  : t.position == "Center" ? "(h-text_h)/2"
                                           : "h-text_h-h*0.08";
    QString y = baseY;
    if (!still && t.animation == "Slide up")
        y = QString("%1+(1-min(1,(t-%2)/0.4))*h*0.05").arg(baseY, num(S));
    QString alpha = "1";
    if (!still && t.animation == "Fade")
        alpha = QString("if(lt(t,%1+0.4),(t-%1)/0.4,if(gt(t,%2-0.4),(%2-t)/0.4,1))").arg(num(S), num(E));

    QString d = QString("drawtext=fontfile='%1':textfile='%2':fontsize=%3:fontcolor=%4:expansion=none")
                    .arg(fontPath(st.font), escapePath(file)).arg(fs).arg(st.color);
    if (st.borderRel > 0)
        d += QString(":borderw=%1:bordercolor=%2").arg(std::max(1, int(fs * st.borderRel))).arg(st.border);
    if (st.box)
        d += QString(":box=1:boxcolor=black@0.6:boxborderw=%1").arg(int(fs * 0.3));
    else
        d += ":shadowx=2:shadowy=2:shadowcolor=black@0.6";
    if (still)          // the caller already filtered to the titles on screen at this moment
        d += QString(":x='(w-text_w)/2':y='%1'").arg(y);
    else
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
    const QString fit  = fitFilter(W, H, s.fill, flags);

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
    for (int i = 0; i < texts.size(); ++i) dts << textFilter(texts[i], i, s.height(), false);
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

QStringList Exporter::buildStillArgs(const Track& track, const QVector<TextItem>& texts,
                                     double atTime, const ExportSettings& s, const QString& output,
                                     int previewWidth) {
    const auto& clips = track.clips();
    if (clips.isEmpty()) return {};
    const double t = std::clamp(atTime, 0.0, std::max(0.0, track.duration() - 0.001));
    const int i = track.clipAt(t);
    if (i < 0 || i >= clips.size()) return {};

    // small version of the real frame shape
    const double k = std::max(240, previewWidth) / double(std::max(s.width(), s.height()));
    const int PW = ExportSettings::even(std::max(2, int(s.width() * k)));
    const int PH = ExportSettings::even(std::max(2, int(s.height() * k)));
    const QString flags = (s.upscale == "None") ? "bicubic" : "lanczos";
    const QString fit  = fitFilter(PW, PH, s.fill, flags);
    const QString pre  = s.upscale.contains("clean")   ? "hqdn3d=1.5:1.5:6:6," : "";
    const QString post = s.upscale.contains("sharpen") ? ",unsharp=5:5:0.8:5:5:0.0" : "";

    // titles that are on screen at this moment (drawn without the fade/slide maths)
    QStringList dts;
    for (const TextItem& tx : texts)
        if (t + 0.0001 >= tx.start && t <= tx.start + tx.duration)
            dts << textFilter(tx, dts.size(), PH, true);
    const QString textChain = dts.isEmpty() ? QString() : ("," + dts.join(","));

    const Clip& c = clips[i];
    const double ov = (i > 0) ? track.joinOverlap(i) : 0.0;
    const bool inTransition = i > 0 && c.transition != "None" && ov > 0.05 &&
                              t < track.clipStart(i) + ov - 0.002;

    QStringList args{"-y", "-nostdin", "-loglevel", "error"};
    QString g;

    if (inTransition) {
        const Clip& prev = clips[i - 1];
        // How far into the transition the playhead is (0 = just started, 1 = finished).
        const double p = std::clamp((t - track.clipStart(i)) / ov, 0.0, 0.95);
        const double prevSrc = prev.sourceIn + std::max(0.0, t - track.clipStart(i - 1)) * prev.speed;
        const double curSrc  = c.sourceIn + std::max(0.0, t - track.clipStart(i)) * c.speed;
        // The exact source frames that are being blended at this moment (speed-aware).
        const double prevAt = std::clamp(prevSrc + p * ov * prev.speed,
                                         prev.sourceIn, std::max(prev.sourceIn, prev.sourceOut - 0.02));
        const double curAt  = std::clamp(curSrc + p * ov * c.speed,
                                         c.sourceIn, std::max(c.sourceIn, c.sourceOut - 0.02));
        args << "-ss" << num(prevAt) << "-i" << prev.path;
        args << "-ss" << num(curAt)  << "-i" << c.path;
        // Freeze both frames, then blend them with the very transition the export uses and
        // grab the image at the same point in the blend. Works for any speed and any length.
        const QString freeze = ",trim=end_frame=1,setpts=PTS-STARTPTS,loop=loop=-1:size=1:start=0,"
                               "trim=duration=1.5,setpts=PTS-STARTPTS,fps=25,setsar=1,format=yuv420p";
        g += QString("[0:v]%1%2%3%4%5[a];").arg(pre, fit, post, effectFilters(prev), freeze);
        g += QString("[1:v]%1%2%3%4%5[b];").arg(pre, fit, post, effectFilters(c), freeze);
        g += QString("[a][b]xfade=transition=%1:duration=1:offset=0,"
                     "trim=start=%2:end=%3,setpts=PTS-STARTPTS,format=rgba%4[v];")
                 .arg(c.transition, num(p), num(std::min(0.999, p + 0.05)), textChain);
        args << "-filter_complex" << g << "-map" << "[v]" << "-frames:v" << "1" << output;
        return args;
    }

    // plain frame: clip look + colour + titles
    const double src = std::clamp(c.sourceIn + std::max(0.0, t - track.clipStart(i)) * c.speed,
                                  c.sourceIn, std::max(c.sourceIn, c.sourceOut - 0.02));
    args << "-ss" << num(src) << "-i" << c.path;
    g = QString("[0:v]%1%2%3%4,setsar=1,format=rgba%5[v]")
            .arg(pre, fit, post, effectFilters(c), textChain);
    args << "-filter_complex" << g << "-map" << "[v]" << "-frames:v" << "1" << output;
    return args;
}

void Exporter::start(const Track& track, const QVector<TextItem>& texts,
                     const QString& output, const ExportSettings& s) {
    if (track.clips().isEmpty()) { emit finished(false, "Timeline is empty."); return; }
    if (isRunning()) return;                      // one job at a time
    m_total = track.exportDuration();
    m_log.clear();
    m_lineBuffer.clear();
    m_output = output;
    m_cancelled = false;
    m_proc.start("ffmpeg", buildArgs(track, texts, output, s));
}

void Exporter::cancel() {
    if (m_proc.state() == QProcess::NotRunning) return;
    m_cancelled = true;
    m_proc.kill();
}

void Exporter::onReadyRead() {
    const QString text = QString::fromUtf8(m_proc.readAll());
    m_log += text;
    if (m_log.size() > 400000) m_log = m_log.right(200000);   // keep the tail for error messages
    m_lineBuffer += text;
    int nl;
    while ((nl = m_lineBuffer.indexOf('\n')) >= 0) {
        const QString line = m_lineBuffer.left(nl).trimmed();
        m_lineBuffer.remove(0, nl + 1);
        if (line.startsWith("out_time_us=") && m_total > 0) {
            bool ok = false;
            const double us = line.mid(12).toDouble(&ok);
            if (ok) emit progress(qBound(0.0, us / 1e6 / m_total, 1.0));
        }
    }
}
