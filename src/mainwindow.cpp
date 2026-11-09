#include "mainwindow.h"
#include "dialogs.h"
#include "version.h"
#include <QMenuBar>
#include <QTimer>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QUrl>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <algorithm>

MainWindow::MainWindow() {
    setWindowTitle(QString("OpenClip %1").arg(OPENCLIP_VERSION));
    resize(1200, 780);

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOut);
    m_video = new QVideoWidget;
    m_video->setMinimumHeight(320);
    m_player->setVideoOutput(m_video);

    m_tl = new TimelineWidget(&track(), &m_texts);
    auto* scroll = new QScrollArea;
    scroll->setWidget(m_tl);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(160);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 1000);
    m_timeLabel = new QLabel("0.00 / 0.00 s");

    auto* tmpl = new QPushButton("Templates...");
    auto* add = new QPushButton("Add clips");
    auto* play = new QPushButton("Play / Pause (Space)");
    auto* split = new QPushButton("Split (S)");
    auto* edit = new QPushButton("Edit clip...");
    auto* text = new QPushButton("Add text...");
    auto* del = new QPushButton("Delete (Del)");
    auto* prev = new QPushButton("Quick preview");
    auto* exp = new QPushButton("Export...");

    auto* bar = new QHBoxLayout;
    for (auto* b : {tmpl, add, play, split, edit, text, del}) bar->addWidget(b);
    bar->addStretch();
    bar->addWidget(m_timeLabel);
    bar->addStretch();
    bar->addWidget(prev);
    bar->addWidget(exp);

    auto* root = new QVBoxLayout;
    root->addWidget(m_video, 1);
    root->addLayout(bar);
    root->addWidget(scroll);
    root->addWidget(m_bar);
    auto* central = new QWidget;
    central->setLayout(root);
    setCentralWidget(central);

    m_updater = new Updater(this);
    auto* help = menuBar()->addMenu("Help");
    help->addAction("Check for updates...", this, [this] { m_updater->check(true); });
    help->addAction("About OpenClip", this, [this] {
        QMessageBox::about(this, "About OpenClip",
            QString("OpenClip %1\nFree and open-source video editor.").arg(OPENCLIP_VERSION));
    });
    QTimer::singleShot(2500, this, [this] { m_updater->check(false); });   // quiet check at startup

    connect(tmpl, &QPushButton::clicked, this, &MainWindow::newFromTemplate);
    connect(add, &QPushButton::clicked, this, &MainWindow::addClips);
    connect(play, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(split, &QPushButton::clicked, this, &MainWindow::splitAtPlayhead);
    connect(edit, &QPushButton::clicked, this, &MainWindow::editSelected);
    connect(text, &QPushButton::clicked, this, &MainWindow::addText);
    connect(del, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(prev, &QPushButton::clicked, this, &MainWindow::quickPreview);
    connect(exp, &QPushButton::clicked, this, &MainWindow::exportVideo);
    new QShortcut(Qt::Key_Space, this, [this] { togglePlay(); });
    new QShortcut(Qt::Key_S, this, [this] { splitAtPlayhead(); });
    new QShortcut(Qt::Key_Delete, this, [this] { removeSelected(); });

    connect(m_tl, &TimelineWidget::seekRequested, this, [this](double t) { seek(t); });
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::onPosition);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia) applyPending();
    });
    connect(&m_exporter, &Exporter::progress, this, [this](double f) { m_bar->setValue(int(f * 1000)); });
    connect(&m_exporter, &Exporter::finished, this, [this](bool ok, const QString& log) {
        const bool wasPreview = m_previewMode;
        m_previewMode = false;
        if (!ok) { QMessageBox::warning(this, "Export failed", log.right(1500)); return; }
        m_bar->setValue(1000);
        if (wasPreview) QDesktopServices::openUrl(QUrl::fromLocalFile(m_previewPath));
        else QMessageBox::information(this, "Export", "Done!");
    });
}

double MainWindow::probeDuration(const QString& path) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path});
    p.waitForFinished(15000);
    return p.readAllStandardOutput().trimmed().toDouble();
}

bool MainWindow::probeHasAudio(const QString& path) {
    QProcess p;
    p.start("ffprobe", {"-v", "error", "-select_streams", "a:0", "-show_entries",
                        "stream=codec_type", "-of", "csv=p=0", path});
    p.waitForFinished(15000);
    return !p.readAllStandardOutput().trimmed().isEmpty();
}

void MainWindow::addClips() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Add clips", {}, "Video (*.mp4 *.mov *.mkv *.webm *.avi)");
    for (const QString& f : files) {
        const double d = probeDuration(f);
        if (d <= 0) {
            QMessageBox::warning(this, "Import", "Could not read: " + f + "\n(is ffprobe installed?)");
            continue;
        }
        Clip c;
        c.path = f; c.sourceIn = 0; c.sourceOut = d; c.hasAudio = probeHasAudio(f);
        track().clips().append(c);
    }
    const bool first = m_currentClip < 0 && !track().clips().isEmpty();
    m_tl->refresh();
    if (first) seek(0);
}

// ---- playback -------------------------------------------------------------

void MainWindow::togglePlay() {
    if (track().clips().isEmpty()) return;
    if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
    else {
        if (m_tl->playhead() >= track().duration() - 0.05) seek(0, true);
        else m_player->play();
    }
}

void MainWindow::seek(double t, bool play) {
    const int i = track().clipAt(t);
    if (i < 0) return;
    const Clip& c = track().clips()[i];
    const double src = c.sourceIn + (t - track().clipStart(i)) * c.speed;
    m_tl->setPlayhead(t);
    loadClip(i, src, play);
}

void MainWindow::loadClip(int index, double sourceTime, bool play) {
    const Clip& c = track().clips()[index];
    const QUrl url = QUrl::fromLocalFile(c.path);
    m_currentClip = index;
    m_pendingSeekMs = qint64(sourceTime * 1000);
    m_playAfterLoad = play;
    if (m_player->source() != url) m_player->setSource(url);
    else applyPending();
}

void MainWindow::applyPending() {
    if (m_pendingSeekMs < 0 || m_currentClip < 0 || m_currentClip >= track().clips().size()) return;
    m_player->setPlaybackRate(track().clips()[m_currentClip].speed);
    m_player->setPosition(m_pendingSeekMs);
    m_pendingSeekMs = -1;
    if (m_playAfterLoad) m_player->play(); else m_player->pause();
}

void MainWindow::onPosition(qint64 ms) {
    if (m_currentClip < 0 || m_pendingSeekMs >= 0 || m_currentClip >= track().clips().size()) return;
    const Clip& c = track().clips()[m_currentClip];
    const double src = ms / 1000.0;
    if (src >= c.sourceOut - 0.03) {
        if (m_currentClip + 1 < track().clips().size())
            loadClip(m_currentClip + 1, track().clips()[m_currentClip + 1].sourceIn, true);
        else m_player->pause();
        return;
    }
    const double t = track().clipStart(m_currentClip) + std::max(0.0, src - c.sourceIn) / c.speed;
    m_tl->setPlayhead(t);
    m_timeLabel->setText(QString("%1 / %2 s").arg(t, 0, 'f', 2).arg(track().duration(), 0, 'f', 2));
}

// ---- editing --------------------------------------------------------------

void MainWindow::afterEdit() {
    m_tl->refresh();
    if (track().clips().isEmpty()) { m_player->stop(); m_currentClip = -1; m_tl->setPlayhead(0); return; }
    seek(std::min(m_tl->playhead(), std::max(0.0, track().duration() - 0.01)));
}

void MainWindow::splitAtPlayhead() {
    m_player->pause();
    track().splitAt(m_tl->playhead());
    afterEdit();
}

void MainWindow::removeSelected() {
    const int ti = m_tl->selectedText();
    if (ti >= 0) { m_texts.remove(ti); m_tl->clearSelection(); m_tl->refresh(); return; }
    const int i = m_tl->selectedClip();
    if (i < 0) return;
    m_player->pause();
    track().remove(i);
    m_tl->clearSelection();
    afterEdit();
}

void MainWindow::editSelected() {
    const int i = m_tl->selectedClip();
    if (i < 0) { QMessageBox::information(this, "Edit clip", "Click a clip on the timeline first."); return; }
    Clip c = track().clips()[i];
    if (!editClipDialog(this, c)) return;
    m_player->pause();
    track().clips()[i] = c;
    afterEdit();
}

void MainWindow::addText() {
    TextItem t;
    t.start = m_tl->playhead();
    if (!addTextDialog(this, t)) return;
    m_texts.append(t);
    m_tl->refresh();
}

void MainWindow::exportVideo() {
    if (track().clips().isEmpty()) return;
    if (!exportDialog(this, m_settings)) return;
    const QString out = QFileDialog::getSaveFileName(this, "Export", "output.mp4", "MP4 (*.mp4)");
    if (out.isEmpty()) return;
    m_bar->setValue(0);
    m_previewMode = false;
    m_exporter.start(track(), m_texts, out, m_settings);
}

void MainWindow::quickPreview() {
    if (track().clips().isEmpty()) return;
    ExportSettings s;                       // small and fast, includes effects/transitions/text
    s.base = 480; s.fps = 30; s.crf = 30; s.preset = "ultrafast"; s.upscale = "None";
    s.aspect = m_settings.aspect; s.fill = m_settings.fill;
    m_previewPath = QDir::temp().filePath("openclip_preview.mp4");
    m_bar->setValue(0);
    m_previewMode = true;
    m_exporter.start(track(), m_texts, m_previewPath, s);
}

void MainWindow::newFromTemplate() {
    if (!track().clips().isEmpty() || !m_texts.isEmpty()) {
        if (QMessageBox::question(this, "New from template",
                "This replaces your current project. Continue?") != QMessageBox::Yes) return;
    }
    VideoTemplate t;
    QVector<SlotPick> picks;
    if (!templateDialog(this, t, picks)) return;

    auto probe = [](const QString& p, double& d, bool& a) {
        d = probeDuration(p); a = probeHasAudio(p); return d > 0;
    };
    Track newTrack;
    QVector<TextItem> newTexts;
    ExportSettings s = m_settings;
    const QString err = applyTemplate(t, picks, newTrack, newTexts, s, probe);
    if (!err.isEmpty()) { QMessageBox::warning(this, "Template", err); return; }

    m_player->pause();
    track() = newTrack;
    m_texts = newTexts;
    m_settings = s;
    m_currentClip = -1;
    m_tl->clearSelection();
    m_tl->setPlayhead(0);
    m_tl->refresh();
    seek(0);
}
