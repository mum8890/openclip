#include "mainwindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QHBoxLayout>
#include <QVBoxLayout>

MainWindow::MainWindow() {
    setWindowTitle("OpenClip");
    resize(1100, 720);

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOut);
    m_video = new QVideoWidget;
    m_video->setMinimumHeight(320);
    m_player->setVideoOutput(m_video);

    m_tl = new TimelineWidget(&track());
    auto* scroll = new QScrollArea;
    scroll->setWidget(m_tl);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(140);

    m_bar = new QProgressBar;
    m_bar->setRange(0, 1000);
    m_timeLabel = new QLabel("0.00 / 0.00 s");
    m_fps = new QComboBox;
    m_fps->addItems({"60 fps", "30 fps", "24 fps"});

    auto* add = new QPushButton("Add clips");
    auto* play = new QPushButton("Play / Pause (Space)");
    auto* split = new QPushButton("Split (S)");
    auto* speed = new QPushButton("Speed...");
    auto* del = new QPushButton("Delete (Del)");
    auto* exp = new QPushButton("Export MP4");

    auto* bar = new QHBoxLayout;
    for (auto* b : {add, play, split, speed, del}) bar->addWidget(b);
    bar->addStretch();
    bar->addWidget(m_timeLabel);
    bar->addStretch();
    bar->addWidget(m_fps);
    bar->addWidget(exp);

    auto* root = new QVBoxLayout;
    root->addWidget(m_video, 1);
    root->addLayout(bar);
    root->addWidget(scroll);
    root->addWidget(m_bar);
    auto* central = new QWidget;
    central->setLayout(root);
    setCentralWidget(central);

    connect(add, &QPushButton::clicked, this, &MainWindow::addClips);
    connect(play, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(split, &QPushButton::clicked, this, &MainWindow::splitAtPlayhead);
    connect(speed, &QPushButton::clicked, this, &MainWindow::setSpeedOfSelected);
    connect(del, &QPushButton::clicked, this, &MainWindow::removeSelected);
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
        if (ok) { m_bar->setValue(1000); QMessageBox::information(this, "Export", "Done!"); }
        else QMessageBox::warning(this, "Export failed", log.right(1500));
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
    if (m_player->source() != url) m_player->setSource(url);   // applyPending runs once loaded
    else applyPending();
}

void MainWindow::applyPending() {
    if (m_pendingSeekMs < 0 || m_currentClip < 0 || m_currentClip >= track().clips().size()) return;
    m_player->setPlaybackRate(track().clips()[m_currentClip].speed);
    m_player->setPosition(m_pendingSeekMs);
    m_pendingSeekMs = -1;
    if (m_playAfterLoad) m_player->play(); else m_player->pause();   // pause() shows the frame
}

void MainWindow::onPosition(qint64 ms) {
    if (m_currentClip < 0 || m_pendingSeekMs >= 0 || m_currentClip >= track().clips().size()) return;
    const Clip& c = track().clips()[m_currentClip];
    const double src = ms / 1000.0;
    if (src >= c.sourceOut - 0.03) {                       // reached end of this clip
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
    const int i = m_tl->selectedClip();
    if (i < 0) return;
    m_player->pause();
    track().remove(i);
    m_tl->clearSelection();
    afterEdit();
}

void MainWindow::setSpeedOfSelected() {
    const int i = m_tl->selectedClip();
    if (i < 0) { QMessageBox::information(this, "Speed", "Click a clip on the timeline first."); return; }
    bool ok = false;
    const double s = QInputDialog::getDouble(this, "Speed", "Speed (0.5 = slow-mo, 2 = fast):",
                                             track().clips()[i].speed, 0.5, 2.0, 2, &ok);
    if (!ok) return;
    m_player->pause();
    track().setSpeed(i, s);
    afterEdit();
}

void MainWindow::exportVideo() {
    if (track().clips().isEmpty()) return;
    const QString out = QFileDialog::getSaveFileName(this, "Export", "output.mp4", "MP4 (*.mp4)");
    if (out.isEmpty()) return;
    m_bar->setValue(0);
    m_exporter.start(track(), out, m_fps->currentText().split(' ').first().toInt());
}
