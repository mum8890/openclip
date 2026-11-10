#include "mainwindow.h"
#include "dialogs.h"
#include "version.h"
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString friendlyTransition(const QString& n) {
    static const QHash<QString, QString> m{
        {"None", "None (plain cut)"}, {"fade", "Fade"}, {"fadeblack", "Fade through black"},
        {"fadewhite", "Fade through white"}, {"dissolve", "Dissolve"}, {"wipeleft", "Wipe left"},
        {"wiperight", "Wipe right"}, {"slideleft", "Slide left"}, {"slideright", "Slide right"},
        {"circleopen", "Circle open"}, {"circleclose", "Circle close"}, {"pixelize", "Pixelate"},
        {"radial", "Radial wipe"}, {"zoomin", "Zoom in"}, {"hblur", "Blur"}};
    return m.value(n, n);
}

QIcon glyphIcon(bool pause) {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(8, 8, 12));
    if (pause) { p.drawRoundedRect(15, 13, 13, 38, 3, 3); p.drawRoundedRect(36, 13, 13, 38, 3, 3); }
    else { QPolygon t; t << QPoint(22, 11) << QPoint(22, 53) << QPoint(55, 32); p.drawPolygon(t); }
    return QIcon(pm);
}

QString clock(double t) {
    t = std::max(0.0, t);
    const int m = int(t) / 60;
    return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(t - m * 60, 4, 'f', 1, QChar('0'));
}

SliderRow makeSliderRow(const QString& title, double lo, double hi, double step, int decimals,
                        const QString& suffix = {}) {
    SliderRow r;
    r.scale = 1.0 / step; r.decimals = decimals; r.suffix = suffix;
    r.box = new QWidget;
    auto* v = new QVBoxLayout(r.box);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(4);
    auto* top = new QHBoxLayout;
    auto* t = new QLabel(title); t->setObjectName("muted");
    r.valueLabel = new QLabel;
    top->addWidget(t); top->addStretch(); top->addWidget(r.valueLabel);
    r.slider = new QSlider(Qt::Horizontal);
    r.slider->setRange(int(std::lround(lo * r.scale)), int(std::lround(hi * r.scale)));
    v->addLayout(top); v->addWidget(r.slider);
    QObject::connect(r.slider, &QSlider::valueChanged, r.box, [r](int) mutable { r.refreshLabel(); });
    return r;
}

QLabel* sectionLabel(const QString& s) { auto* l = new QLabel(s.toUpper()); l->setObjectName("muted"); return l; }

QWidget* scrolled(QWidget* page) {
    auto* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(page);
    return sa;
}

} // namespace

// ---------------------------------------------------------------------------------------------
MainWindow::MainWindow() {
    resize(1440, 900);
    setMinimumSize(1100, 700);
    setDockNestingEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);     // timeline spans the full width
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    m_player = new QMediaPlayer(this);
    m_audioOut = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOut);
    m_video = new QVideoWidget;
    m_video->setMinimumSize(320, 180);
    m_video->setStyleSheet("background:#000;");
    m_player->setVideoOutput(m_video);
    m_tl = new TimelineWidget(&track(), &m_texts);
    m_updater = new Updater(this);
    m_bar = new QProgressBar;
    m_bar->setRange(0, 1000);
    m_bar->setFixedWidth(240);
    m_bar->setTextVisible(false);

    // top bar (as a toolbar so it spans the whole window)
    auto* tb = new QToolBar;
    tb->setMovable(false); tb->setFloatable(false);
    tb->addWidget(buildTopBar());
    addToolBar(Qt::TopToolBarArea, tb);

    // centre = the program monitor
    auto* centre = new QWidget;
    auto* cl = new QVBoxLayout(centre);
    cl->setContentsMargins(6, 6, 6, 6);
    cl->addWidget(buildPlayer());
    setCentralWidget(centre);

    // panels you can drag around, tab together, float or close
    m_dProject = makeDock("Project", "dock_project", buildMediaPanel());
    m_dEffects = makeDock("Effects", "dock_effects",
        buildListPanel("Click a clip on the timeline, then click a look to apply it.", kEffects, false,
                       [this](const QString& s) { applyEffect(s); }));
    m_dTransitions = makeDock("Transitions", "dock_transitions",
        buildListPanel("Click a clip (not the first one), then click a transition. It blends from the clip before it.",
                       kTransitions, true, [this](const QString& s) { applyTransition(s); }));
    m_dTitles = makeDock("Titles", "dock_titles",
        buildListPanel("Move the white line to where you want the title, then click a style. Edit the words in Properties.",
                       kTextStyles, false, [this](const QString& s) { addTextWithStyle(s); }));
    m_dTemplates = makeDock("Templates", "dock_templates", buildTemplatesPanel());
    m_dProps = makeDock("Properties", "dock_props", buildInspector());
    m_dTimeline = makeDock("Timeline", "dock_timeline", buildTimelinePanel());

    buildMenus();
    statusBar()->addPermanentWidget(m_bar);
    statusBar()->showMessage("Welcome! Open the Project panel, import a video, then double-click it to add it to the timeline.");

    connect(m_tl, &TimelineWidget::seekRequested, this, [this](double t) { seek(t); });
    connect(m_tl, &TimelineWidget::selectionChanged, this, &MainWindow::updateInspector);
    connect(m_tl, &TimelineWidget::zoomChanged, this, [this](double pps) { QSignalBlocker b(m_zoom); m_zoom->setValue(int(pps)); });
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::onPosition);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        m_playBtn->setIcon(glyphIcon(s == QMediaPlayer::PlayingState));
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia) applyPending();
    });
    connect(&m_exporter, &Exporter::progress, this, [this](double f) { m_bar->setValue(int(f * 1000)); });
    connect(&m_exporter, &Exporter::finished, this, [this](bool ok, const QString& log) {
        const bool wasPreview = m_previewMode;
        m_previewMode = false;
        if (!ok) { QMessageBox::warning(this, "Export failed", log.right(1500)); statusBar()->showMessage("Export failed."); return; }
        m_bar->setValue(1000);
        if (wasPreview) { statusBar()->showMessage("Preview ready."); QDesktopServices::openUrl(QUrl::fromLocalFile(m_previewPath)); }
        else { statusBar()->showMessage("Export finished."); QMessageBox::information(this, "Export", "Done!"); }
    });
    QTimer::singleShot(2500, this, [this] { m_updater->check(false); });

    // layout: remember the user's arrangement between sessions
    applyWorkspace("Editing", false);
    QSettings settings("OpenClip", "OpenClip");
    const bool restored = settings.contains("layout/state") && restoreState(settings.value("layout/state").toByteArray(), 1);
    if (!restored) QTimer::singleShot(0, this, [this] { applyWorkspace("Editing", true); });

    updateInspector();
    updateTimeUi(0);
    updateTitle();
}

Clip* MainWindow::selClip() {
    const int i = m_tl->selectedClip();
    return (i >= 0 && i < track().clips().size()) ? &track().clips()[i] : nullptr;
}
TextItem* MainWindow::selText() {
    const int i = m_tl->selectedText();
    return (i >= 0 && i < m_texts.size()) ? &m_texts[i] : nullptr;
}

Clip* MainWindow::editClip(const QString& key) {
    if (!selClip()) return nullptr;
    checkpoint(key);
    return selClip();
}
TextItem* MainWindow::editText(const QString& key) {
    if (!selText()) return nullptr;
    checkpoint(key);
    return selText();
}

// ---- docks & workspaces ---------------------------------------------------------------------------

QDockWidget* MainWindow::makeDock(const QString& title, const QString& objectName, QWidget* content) {
    auto* d = new QDockWidget(title, this);
    d->setObjectName(objectName);
    d->setWidget(content);
    d->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    // custom title bar: name on the left, a close button on the right (drag it to move the panel)
    auto* bar = new QWidget; bar->setObjectName("docktitle");
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(12, 6, 6, 6);
    auto* name = new QLabel(title); name->setObjectName("title");
    auto* x = new QPushButton(QString::fromUtf8("\xC3\x97")); x->setObjectName("flat"); x->setToolTip("Close panel (reopen it from the Window menu)");
    x->setFixedSize(26, 26);
    l->addWidget(name); l->addStretch(); l->addWidget(x);
    connect(x, &QPushButton::clicked, d, &QDockWidget::close);
    d->setTitleBarWidget(bar);
    return d;
}

void MainWindow::applyWorkspace(const QString& name, bool withSizes) {
    const QList<QDockWidget*> all{m_dProject, m_dEffects, m_dTransitions, m_dTitles, m_dTemplates, m_dProps, m_dTimeline};
    for (auto* d : all) { d->setFloating(false); removeDockWidget(d); }
    addDockWidget(Qt::LeftDockWidgetArea, m_dProject);
    for (auto* d : {m_dEffects, m_dTransitions, m_dTitles, m_dTemplates}) {
        addDockWidget(Qt::LeftDockWidgetArea, d);
        tabifyDockWidget(m_dProject, d);
    }
    addDockWidget(Qt::RightDockWidgetArea, m_dProps);
    addDockWidget(Qt::BottomDockWidgetArea, m_dTimeline);
    for (auto* d : all) d->show();

    QDockWidget* front = m_dProject;
    int left = 380, right = 320, bottom = 300;
    if (name == "Assembly") { left = 560; bottom = 340; }
    else if (name == "Effects") front = m_dEffects;
    else if (name == "Titles") front = m_dTitles;
    front->raise();
    if (name == "Assembly") m_dProps->hide();

    if (withSizes) {
        resizeDocks({m_dProject, m_dProps}, {left, right}, Qt::Horizontal);
        resizeDocks({m_dTimeline}, {bottom}, Qt::Vertical);
    }
}

// ---- menus --------------------------------------------------------------------------------------------

void MainWindow::buildMenus() {
    auto* mb = menuBar();

    auto* file = mb->addMenu("File");
    file->addAction("New Project", this, &MainWindow::newProject)->setShortcut(QKeySequence::New);
    file->addAction("Open Project...", this, &MainWindow::openProject)->setShortcut(QKeySequence::Open);
    file->addAction("Save", this, [this] { saveProject(false); })->setShortcut(QKeySequence::Save);
    file->addAction("Save As...", this, [this] { saveProject(true); })->setShortcut(QKeySequence("Ctrl+Shift+S"));
    file->addSeparator();
    file->addAction("Import Media...", this, &MainWindow::importMedia)->setShortcut(QKeySequence("Ctrl+I"));
    file->addAction("New from Template...", this, &MainWindow::newFromTemplate);
    file->addSeparator();
    file->addAction("Quick Preview", this, &MainWindow::quickPreview);
    file->addAction("Export...", this, &MainWindow::exportVideo)->setShortcut(QKeySequence("Ctrl+M"));
    file->addSeparator();
    file->addAction("Exit", this, &QWidget::close)->setShortcut(QKeySequence("Ctrl+Q"));

    auto* edit = mb->addMenu("Edit");
    edit->addAction("Undo", this, &MainWindow::undo)->setShortcut(QKeySequence::Undo);
    auto* redoA = edit->addAction("Redo", this, &MainWindow::redo);
    redoA->setShortcuts({QKeySequence::Redo, QKeySequence("Ctrl+Y")});
    edit->addSeparator();
    edit->addAction("Split at Playhead", this, &MainWindow::splitAtPlayhead)->setShortcut(QKeySequence(Qt::Key_S));
    edit->addAction("Delete", this, &MainWindow::removeSelected)->setShortcut(QKeySequence(Qt::Key_Delete));

    auto* clip = mb->addMenu("Clip");
    auto* fx = clip->addMenu("Apply Effect");
    for (const QString& e : kEffects) fx->addAction(e, this, [this, e] { applyEffect(e); });
    auto* tr = clip->addMenu("Apply Transition");
    for (const QString& t : kTransitions) tr->addAction(friendlyTransition(t), this, [this, t] { applyTransition(t); });
    auto* sp = clip->addMenu("Speed");
    for (double s : {0.5, 0.75, 1.0, 1.25, 1.5, 2.0}) sp->addAction(QString("%1x").arg(s), this, [this, s] { applySpeed(s); });

    auto* seq = mb->addMenu("Sequence");
    seq->addAction("Play / Pause", this, &MainWindow::togglePlay)->setShortcut(QKeySequence(Qt::Key_Space));
    seq->addAction("Go to Start", this, [this] { if (!track().clips().isEmpty()) seek(0); })->setShortcut(QKeySequence(Qt::Key_Home));
    seq->addAction("Go to End", this, [this] { if (!track().clips().isEmpty()) seek(std::max(0.0, track().duration() - 0.05)); })->setShortcut(QKeySequence(Qt::Key_End));

    auto* gfx = mb->addMenu("Graphics and Titles");
    auto* addT = gfx->addMenu("Add Title");
    for (const QString& s : kTextStyles) addT->addAction(s, this, [this, s] { addTextWithStyle(s); });

    auto* view = mb->addMenu("View");
    view->addAction("Zoom In Timeline", this, [this] { m_zoom->setValue(int(m_zoom->value() * 1.25) + 1); })->setShortcut(QKeySequence("Ctrl+="));
    view->addAction("Zoom Out Timeline", this, [this] { m_zoom->setValue(int(m_zoom->value() / 1.25)); })->setShortcut(QKeySequence("Ctrl+-"));

    auto* win = mb->addMenu("Window");
    auto* ws = win->addMenu("Workspaces");
    for (const QString& n : {"Editing", "Assembly", "Effects", "Titles"})
        ws->addAction(n, this, [this, n] { applyWorkspace(n, true); });
    win->addAction("Reset to Default Layout", this, [this] { applyWorkspace("Editing", true); });
    win->addSeparator();
    for (auto* d : {m_dProject, m_dEffects, m_dTransitions, m_dTitles, m_dTemplates, m_dProps, m_dTimeline})
        win->addAction(d->toggleViewAction());

    auto* help = mb->addMenu("Help");
    help->addAction("Check for Updates...", this, [this] { m_updater->check(true); });
    help->addAction("About OpenClip", this, [this] {
        QMessageBox box(this);
        box.setWindowTitle("About OpenClip");
        box.setIconPixmap(QPixmap(":/ghostreel.png").scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        box.setTextFormat(Qt::RichText);
        box.setTextInteractionFlags(Qt::TextBrowserInteraction);
        box.setText(QString("<b>OpenClip %1</b><br>Free and open-source video editor.<br><br>"
                            "Published by %2<br><a href=\"https://github.com/%3\">github.com/%3</a>")
                        .arg(OPENCLIP_VERSION, OPENCLIP_PUBLISHER, OPENCLIP_REPO));
        box.exec();
    });
}

// ---- panel builders ---------------------------------------------------------------------------------

QWidget* MainWindow::buildTopBar() {
    auto* bar = new QWidget; bar->setObjectName("topbar");
    bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(16, 8, 16, 8); l->setSpacing(10);
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(":/icon.png").scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* brand = new QLabel("OpenClip"); brand->setObjectName("brand");
    auto* ver = new QLabel(QString("v%1").arg(OPENCLIP_VERSION)); ver->setObjectName("muted");
    m_projName = new QLabel("Untitled"); m_projName->setObjectName("title");
    auto* prev = new QPushButton("Quick preview");
    auto* exp = new QPushButton("Export"); exp->setObjectName("accent");
    l->addWidget(logo); l->addWidget(brand); l->addWidget(ver);
    l->addStretch(); l->addWidget(m_projName); l->addStretch();
    l->addWidget(prev); l->addWidget(exp);
    connect(prev, &QPushButton::clicked, this, &MainWindow::quickPreview);
    connect(exp, &QPushButton::clicked, this, &MainWindow::exportVideo);
    return bar;
}

QWidget* MainWindow::buildPlayer() {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(8);
    m_previewStack = new QStackedWidget;
    auto* ph = new QWidget; ph->setObjectName("panel");
    auto* phl = new QVBoxLayout(ph);
    auto* phLogo = new QLabel; phLogo->setAlignment(Qt::AlignCenter);
    phLogo->setPixmap(QPixmap(":/icon.png").scaled(84, 84, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* phText = new QLabel("Import a video in the Project panel,\nthen double-click it to start editing");
    phText->setObjectName("hint"); phText->setAlignment(Qt::AlignCenter);
    phl->addStretch(); phl->addWidget(phLogo); phl->addSpacing(8); phl->addWidget(phText); phl->addStretch();
    m_previewStack->addWidget(ph);
    m_previewStack->addWidget(m_video);
    v->addWidget(m_previewStack, 1);

    auto* bar = new QWidget; bar->setObjectName("playerbar");
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(12, 8, 14, 8); l->setSpacing(12);
    m_playBtn = new QPushButton; m_playBtn->setObjectName("round");
    m_playBtn->setIcon(glyphIcon(false)); m_playBtn->setIconSize(QSize(20, 20));
    m_playBtn->setToolTip("Play / Pause (Space)");
    m_curTime = new QLabel("00:00.0"); m_totTime = new QLabel("00:00.0"); m_totTime->setObjectName("muted");
    m_seek = new QSlider(Qt::Horizontal);
    l->addWidget(m_playBtn); l->addWidget(m_curTime); l->addWidget(m_seek, 1); l->addWidget(m_totTime);
    v->addWidget(bar);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(m_seek, &QSlider::sliderMoved, this, [this](int v) { seek(v / 100.0); });
    return w;
}

QWidget* MainWindow::buildMediaPanel() {
    auto* media = new QWidget; media->setObjectName("panel");
    auto* mv = new QVBoxLayout(media);
    mv->setContentsMargins(10, 10, 10, 10); mv->setSpacing(8);
    auto* imp = new QPushButton("+  Import media"); imp->setObjectName("accent");
    m_media = new QListWidget;
    m_media->setSelectionMode(QAbstractItemView::ExtendedSelection);
    auto* addSel = new QPushButton("Add selected to timeline");
    auto* mh = new QLabel("Double-click a video to add it to the timeline."); mh->setObjectName("hint"); mh->setWordWrap(true);
    mv->addWidget(imp); mv->addWidget(m_media, 1); mv->addWidget(addSel); mv->addWidget(mh);
    connect(imp, &QPushButton::clicked, this, &MainWindow::importMedia);
    connect(addSel, &QPushButton::clicked, this, &MainWindow::addSelectedMedia);
    connect(m_media, &QListWidget::itemDoubleClicked, this, &MainWindow::addMediaItem);
    return media;
}

QWidget* MainWindow::buildListPanel(const QString& hint, const QStringList& items, bool transitions,
                                    std::function<void(const QString&)> onClick) {
    auto* page = new QWidget; page->setObjectName("panel");
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(10, 10, 10, 10); v->setSpacing(8);
    auto* h = new QLabel(hint); h->setObjectName("hint"); h->setWordWrap(true);
    auto* list = new QListWidget;
    for (const QString& n : items) {
        auto* it = new QListWidgetItem(transitions ? friendlyTransition(n) : n);
        it->setData(Qt::UserRole, n);
        list->addItem(it);
    }
    v->addWidget(h); v->addWidget(list, 1);
    connect(list, &QListWidget::itemClicked, this, [onClick](QListWidgetItem* it) { onClick(it->data(Qt::UserRole).toString()); });
    return page;
}

QWidget* MainWindow::buildTemplatesPanel() {
    auto* tp = new QWidget; tp->setObjectName("panel");
    auto* tv = new QVBoxLayout(tp);
    tv->setContentsMargins(10, 10, 10, 10); tv->setSpacing(10);
    auto* th = new QLabel("Start from a ready-made edit: pick a template, choose a video for each part, and OpenClip builds the project with effects, transitions and titles already set.");
    th->setObjectName("hint"); th->setWordWrap(true);
    auto* tb = new QPushButton("Browse templates..."); tb->setObjectName("accent");
    auto* names = new QLabel("• Gameplay Highlight\n• Vertical Short\n• Kill Montage\n• Cinematic Trailer");
    names->setObjectName("hint");
    tv->addWidget(th); tv->addWidget(tb); tv->addWidget(names); tv->addStretch();
    connect(tb, &QPushButton::clicked, this, &MainWindow::newFromTemplate);
    return tp;
}

QWidget* MainWindow::buildInspector() {
    m_inspector = new QStackedWidget;

    auto* empty = new QWidget;
    auto* ev = new QVBoxLayout(empty);
    auto* eh = new QLabel("Select a clip or a title on the timeline to edit it here.\n\nTip: use the Effects and Transitions panels to apply a look quickly.");
    eh->setObjectName("hint"); eh->setWordWrap(true); eh->setAlignment(Qt::AlignCenter);
    ev->addWidget(eh);
    m_inspector->addWidget(empty);

    auto* cp = new QWidget;
    auto* cv = new QVBoxLayout(cp);
    cv->setContentsMargins(14, 14, 14, 14); cv->setSpacing(10);
    auto* ct = new QLabel("Clip"); ct->setObjectName("title");
    m_clipName = new QLabel; m_clipName->setObjectName("muted"); m_clipName->setWordWrap(true);
    m_speed = makeSliderRow("Speed", 0.5, 2.0, 0.05, 2, "x");
    m_effectBox = new QComboBox; m_effectBox->addItems(kEffects);
    m_bright = makeSliderRow("Brightness", -1.0, 1.0, 0.05, 2);
    m_contrast = makeSliderRow("Contrast", 0.0, 3.0, 0.05, 2);
    m_sat = makeSliderRow("Saturation", 0.0, 3.0, 0.05, 2);
    m_transBox = new QComboBox;
    for (const QString& t : kTransitions) m_transBox->addItem(friendlyTransition(t), t);
    m_transDur = makeSliderRow("Transition length", 0.1, 3.0, 0.1, 1, " s");
    cv->addWidget(ct); cv->addWidget(m_clipName);
    cv->addWidget(m_speed.box);
    cv->addWidget(sectionLabel("Effect")); cv->addWidget(m_effectBox);
    cv->addWidget(sectionLabel("Colour")); cv->addWidget(m_bright.box); cv->addWidget(m_contrast.box); cv->addWidget(m_sat.box);
    cv->addWidget(sectionLabel("Transition from previous clip")); cv->addWidget(m_transBox); cv->addWidget(m_transDur.box);
    cv->addStretch();
    m_inspector->addWidget(scrolled(cp));

    auto* tp = new QWidget;
    auto* tv = new QVBoxLayout(tp);
    tv->setContentsMargins(14, 14, 14, 14); tv->setSpacing(10);
    auto* tt = new QLabel("Title"); tt->setObjectName("title");
    m_textEdit = new QLineEdit;
    m_styleBox = new QComboBox; m_styleBox->addItems(kTextStyles);
    m_posBox = new QComboBox; m_posBox->addItems(kTextPositions);
    m_animBox = new QComboBox; m_animBox->addItems(kTextAnimations);
    m_startSpin = new QDoubleSpinBox; m_startSpin->setRange(0, 36000); m_startSpin->setDecimals(1); m_startSpin->setSuffix(" s");
    m_durSpin = new QDoubleSpinBox; m_durSpin->setRange(0.5, 600); m_durSpin->setDecimals(1); m_durSpin->setSuffix(" s");
    m_sizeRow = makeSliderRow("Size", 3, 30, 1, 0, " %");
    tv->addWidget(tt);
    tv->addWidget(sectionLabel("Text")); tv->addWidget(m_textEdit);
    tv->addWidget(sectionLabel("Style")); tv->addWidget(m_styleBox);
    tv->addWidget(sectionLabel("Position")); tv->addWidget(m_posBox);
    tv->addWidget(sectionLabel("Animation")); tv->addWidget(m_animBox);
    tv->addWidget(sectionLabel("Starts at")); tv->addWidget(m_startSpin);
    tv->addWidget(sectionLabel("Shown for")); tv->addWidget(m_durSpin);
    tv->addWidget(m_sizeRow.box);
    tv->addStretch();
    m_inspector->addWidget(scrolled(tp));

    // ---- live editing: every control writes straight into the selected clip / title ----
    auto commitSpeed = [this]() {
        if (m_inspLock) return;
        if (Clip* c = editClip("insp-speed")) { c->speed = m_speed.value(); m_player->pause(); afterEdit(); }
    };
    connect(m_speed.slider, &QSlider::sliderReleased, this, commitSpeed);
    connect(m_speed.slider, &QSlider::valueChanged, this, [this, commitSpeed](int) { if (!m_speed.slider->isSliderDown()) commitSpeed(); });
    connect(m_effectBox, &QComboBox::currentTextChanged, this, [this](const QString& t) {
        if (m_inspLock) return; if (Clip* c = editClip("insp-effect")) { c->effect = t; m_tl->update(); } });
    auto numeric = [this](SliderRow& row, double Clip::*field, const char* key) {
        connect(row.slider, &QSlider::valueChanged, this, [this, &row, field, key](int) {
            if (m_inspLock) return; if (Clip* c = editClip(key)) { c->*field = row.value(); m_tl->update(); } });
    };
    numeric(m_bright, &Clip::brightness, "insp-bright"); numeric(m_contrast, &Clip::contrast, "insp-contrast");
    numeric(m_sat, &Clip::saturation, "insp-sat");       numeric(m_transDur, &Clip::transitionDur, "insp-tdur");
    connect(m_transBox, &QComboBox::currentIndexChanged, this, [this](int i) {
        if (m_inspLock || i < 0) return;
        if (Clip* c = editClip("insp-trans")) { c->transition = m_transBox->itemData(i).toString(); m_tl->update(); } });

    connect(m_textEdit, &QLineEdit::textEdited, this, [this](const QString& s) {
        if (TextItem* t = editText("insp-text")) { t->text = s; m_tl->update(); } });
    connect(m_styleBox, &QComboBox::currentTextChanged, this, [this](const QString& s) { if (!m_inspLock) if (TextItem* t = editText("insp-style")) { t->style = s; } });
    connect(m_posBox, &QComboBox::currentTextChanged, this, [this](const QString& s) { if (!m_inspLock) if (TextItem* t = editText("insp-pos")) { t->position = s; } });
    connect(m_animBox, &QComboBox::currentTextChanged, this, [this](const QString& s) { if (!m_inspLock) if (TextItem* t = editText("insp-anim")) { t->animation = s; } });
    connect(m_startSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { if (!m_inspLock) if (TextItem* t = editText("insp-start")) { t->start = v; m_tl->refresh(); } });
    connect(m_durSpin, &QDoubleSpinBox::valueChanged, this, [this](double v) { if (!m_inspLock) if (TextItem* t = editText("insp-dur")) { t->duration = v; m_tl->refresh(); } });
    connect(m_sizeRow.slider, &QSlider::valueChanged, this, [this](int v) { if (!m_inspLock) if (TextItem* t = editText("insp-size")) { t->sizePct = v; } });

    auto* panel = new QWidget; panel->setObjectName("panel");
    auto* l = new QVBoxLayout(panel);
    l->setContentsMargins(0, 4, 0, 4);
    l->addWidget(m_inspector);
    return panel;
}

QWidget* MainWindow::buildTimelinePanel() {
    auto* panel = new QWidget; panel->setObjectName("panel");
    auto* v = new QVBoxLayout(panel);
    v->setContentsMargins(8, 8, 8, 8); v->setSpacing(6);

    auto* bar = new QHBoxLayout;
    auto* split = new QPushButton("Split"); split->setToolTip("Cut the clip at the white line (S)");
    auto* del = new QPushButton("Delete"); del->setToolTip("Delete the selected clip or title (Del)");
    auto* undoB = new QPushButton("Undo"); undoB->setToolTip("Undo (Ctrl+Z)");
    auto* redoB = new QPushButton("Redo"); redoB->setToolTip("Redo (Ctrl+Shift+Z)");
    auto* zl = new QLabel("Zoom"); zl->setObjectName("muted");
    m_zoom = new QSlider(Qt::Horizontal);
    m_zoom->setRange(4, 200); m_zoom->setValue(int(m_tl->pixelsPerSecond())); m_zoom->setFixedWidth(160);
    bar->addWidget(split); bar->addWidget(del); bar->addSpacing(12); bar->addWidget(undoB); bar->addWidget(redoB);
    bar->addStretch(); bar->addWidget(zl); bar->addWidget(m_zoom);
    v->addLayout(bar);
    connect(split, &QPushButton::clicked, this, &MainWindow::splitAtPlayhead);
    connect(del, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(undoB, &QPushButton::clicked, this, &MainWindow::undo);
    connect(redoB, &QPushButton::clicked, this, &MainWindow::redo);
    connect(m_zoom, &QSlider::valueChanged, this, [this](int z) { m_tl->setPixelsPerSecond(z); });

    auto* sa = new QScrollArea;
    sa->setWidget(m_tl);
    sa->setWidgetResizable(true);
    sa->setMinimumHeight(150);
    v->addWidget(sa, 1);
    return panel;
}

// ---- project files -----------------------------------------------------------------------------------

ProjectData MainWindow::collect() const {
    ProjectData d;
    d.clips = m_timeline.mainTrack().clips();
    d.texts = m_texts;
    d.settings = m_settings;
    for (int i = 0; i < m_media->count(); ++i) {
        const auto* it = m_media->item(i);
        d.media.append({it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toDouble(), it->data(Qt::UserRole + 2).toBool()});
    }
    return d;
}

void MainWindow::setDirty(bool d) { m_dirty = d; updateTitle(); }

void MainWindow::updateTitle() {
    const QString name = m_projectPath.isEmpty() ? "Untitled" : QFileInfo(m_projectPath).completeBaseName();
    setWindowTitle(QString("%1%2 - OpenClip %3").arg(name, m_dirty ? " *" : "", OPENCLIP_VERSION));
    if (m_projName) m_projName->setText(name + (m_dirty ? "  \xE2\x80\xA2" : ""));
}

bool MainWindow::maybeSave() {
    if (!m_dirty) return true;
    if (track().clips().isEmpty() && m_texts.isEmpty()) return true;
    const auto r = QMessageBox::question(this, "Unsaved changes", "Save your project before continuing?",
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Save) return saveProject(false);
    return r == QMessageBox::Discard;
}

void MainWindow::newProject() {
    if (!maybeSave()) return;
    m_player->pause();
    m_player->setSource(QUrl());
    track().clips().clear();
    m_texts.clear();
    m_media->clear();
    m_settings = ExportSettings{};
    m_projectPath.clear();
    m_undo.clear(); m_redo.clear();
    m_currentClip = -1;
    m_tl->clearSelection();
    m_tl->refresh();
    setPlayhead(0);
    setDirty(false);
    statusBar()->showMessage("New project.");
}

bool MainWindow::saveProject(bool saveAs) {
    QString path = m_projectPath;
    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, "Save project", m_projectPath.isEmpty() ? "My project.openclip" : m_projectPath,
                                            "OpenClip project (*.openclip)");
        if (path.isEmpty()) return false;
        if (!path.endsWith(".openclip", Qt::CaseInsensitive)) path += ".openclip";
    }
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(projectToJson(collect())) < 0) {
        QMessageBox::warning(this, "Save project", "Could not save the file:\n" + path);
        return false;
    }
    f.close();
    m_projectPath = path;
    setDirty(false);
    statusBar()->showMessage("Saved " + QFileInfo(path).fileName());
    return true;
}

void MainWindow::openProject() {
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open project", {}, "OpenClip project (*.openclip)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Open project", "Could not read the file."); return; }
    ProjectData pd;
    QString err;
    if (!projectFromJson(f.readAll(), pd, &err)) { QMessageBox::warning(this, "Open project", err); return; }

    m_player->pause();
    m_player->setSource(QUrl());
    track().clips() = pd.clips;
    m_texts = pd.texts;
    m_settings = pd.settings;
    m_media->clear();
    for (const MediaEntry& m : pd.media) addMediaEntry(m.path, m.duration, m.hasAudio);
    m_undo.clear(); m_redo.clear();
    m_projectPath = path;
    m_currentClip = -1;
    m_tl->clearSelection();
    m_tl->refresh();
    if (!track().clips().isEmpty()) seek(0); else setPlayhead(0);
    setDirty(false);

    QStringList missing;
    for (const Clip& c : track().clips()) if (!QFileInfo::exists(c.path) && !missing.contains(c.path)) missing << c.path;
    if (!missing.isEmpty())
        QMessageBox::warning(this, "Missing videos",
            "These video files could not be found (they may have been moved):\n\n" + missing.join("\n"));
    statusBar()->showMessage("Opened " + QFileInfo(path).fileName());
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!maybeSave()) { e->ignore(); return; }
    QSettings settings("OpenClip", "OpenClip");
    settings.setValue("layout/state", saveState(1));
    e->accept();
}

// ---- undo / redo --------------------------------------------------------------------------------------

void MainWindow::checkpoint(const QString& key) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // dragging a slider fires many changes: only the first one in a burst is remembered
    const bool coalesce = key.startsWith("insp-");           // only inspector drags are merged
    if (!coalesce || key != m_lastKey || now - m_lastCheckpointMs > 900) {
        m_undo.append({track().clips(), m_texts});
        if (m_undo.size() > 100) m_undo.removeFirst();
        m_redo.clear();
    }
    m_lastKey = key;
    m_lastCheckpointMs = now;
    if (!m_dirty) setDirty(true);
}

void MainWindow::restore(const Snapshot& s) {
    m_player->pause();
    track().clips() = s.clips;
    m_texts = s.texts;
    m_tl->clearSelection();
    afterEdit();
    m_tl->refresh();
    setDirty(true);
}

void MainWindow::undo() {
    if (m_undo.isEmpty()) { statusBar()->showMessage("Nothing to undo."); return; }
    m_redo.append({track().clips(), m_texts});
    const Snapshot s = m_undo.takeLast();
    m_lastKey.clear();
    restore(s);
    statusBar()->showMessage("Undone.");
}

void MainWindow::redo() {
    if (m_redo.isEmpty()) { statusBar()->showMessage("Nothing to redo."); return; }
    m_undo.append({track().clips(), m_texts});
    const Snapshot s = m_redo.takeLast();
    m_lastKey.clear();
    restore(s);
    statusBar()->showMessage("Redone.");
}

// ---- media library and quick tools ---------------------------------------------------------------------

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

void MainWindow::addMediaEntry(const QString& path, double duration, bool hasAudio) {
    auto* it = new QListWidgetItem(QString("%1\n%2").arg(QFileInfo(path).fileName(), clock(duration)));
    it->setData(Qt::UserRole, path);
    it->setData(Qt::UserRole + 1, duration);
    it->setData(Qt::UserRole + 2, hasAudio);
    it->setToolTip(path);
    m_media->addItem(it);
}

void MainWindow::importMedia() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, "Import media", {}, "Video (*.mp4 *.mov *.mkv *.webm *.avi)");
    for (const QString& f : files) {
        const double d = probeDuration(f);
        if (d <= 0) { QMessageBox::warning(this, "Import", "Could not read: " + f + "\n(is ffprobe installed?)"); continue; }
        addMediaEntry(f, d, probeHasAudio(f));
        setDirty(true);
    }
    statusBar()->showMessage("Imported. Double-click a video to add it to the timeline.");
}

void MainWindow::addMediaItem(QListWidgetItem* item) {
    Clip c;
    c.path = item->data(Qt::UserRole).toString();
    c.sourceIn = 0;
    c.sourceOut = item->data(Qt::UserRole + 1).toDouble();
    c.hasAudio = item->data(Qt::UserRole + 2).toBool();
    if (c.sourceOut <= 0) return;
    checkpoint("add-clip");
    const bool first = track().clips().isEmpty();
    track().clips().append(c);
    m_tl->refresh();
    if (first) seek(0);
    m_tl->selectClip(track().clips().size() - 1);
    updateTimeUi(m_tl->playhead());
}

void MainWindow::addSelectedMedia() {
    const auto items = m_media->selectedItems();
    if (items.isEmpty()) { statusBar()->showMessage("Click a video in the list first."); return; }
    for (auto* it : items) addMediaItem(it);
}

void MainWindow::applyEffect(const QString& name) {
    if (Clip* c = editClip("effect")) {
        c->effect = name; m_tl->update(); updateInspector();
        statusBar()->showMessage("Effect: " + name + ". Use Quick preview to see it.");
    } else statusBar()->showMessage("Click a clip on the timeline first.");
}

void MainWindow::applyTransition(const QString& name) {
    const int i = m_tl->selectedClip();
    if (i < 0) { statusBar()->showMessage("Click a clip on the timeline first."); return; }
    if (i == 0) { statusBar()->showMessage("The first clip has nothing before it. Click a later clip."); return; }
    checkpoint("transition");
    track().clips()[i].transition = name;
    m_tl->update(); updateInspector();
    statusBar()->showMessage("Transition: " + friendlyTransition(name) + ". Use Quick preview to see it.");
}

void MainWindow::applySpeed(double speed) {
    Clip* c = editClip("speed-menu");
    if (!c) { statusBar()->showMessage("Click a clip on the timeline first."); return; }
    c->speed = std::clamp(speed, 0.5, 2.0);
    m_player->pause();
    afterEdit();
    updateInspector();
}

void MainWindow::addTextWithStyle(const QString& style) {
    checkpoint("add-text");
    TextItem t;
    t.start = m_tl->playhead();
    t.style = style;
    m_texts.append(t);
    m_tl->refresh();
    m_tl->selectText(m_texts.size() - 1);
    m_textEdit->setFocus();
    m_textEdit->selectAll();
    statusBar()->showMessage("Title added. Type your words in the Properties panel.");
}

// ---- inspector sync ---------------------------------------------------------------------------------------

void MainWindow::updateInspector() {
    m_inspLock = true;
    if (Clip* c = selClip()) {
        const int ci = m_tl->selectedClip();
        m_clipName->setText(QFileInfo(c->path).fileName());
        m_speed.set(c->speed);
        m_effectBox->setCurrentText(c->effect);
        m_bright.set(c->brightness); m_contrast.set(c->contrast); m_sat.set(c->saturation);
        m_transBox->setCurrentIndex(std::max(0, m_transBox->findData(c->transition)));
        m_transDur.set(c->transitionDur);
        m_transBox->setEnabled(ci > 0); m_transDur.box->setEnabled(ci > 0);
        m_inspector->setCurrentIndex(1);
    } else if (TextItem* t = selText()) {
        m_textEdit->setText(t->text);
        m_styleBox->setCurrentText(t->style); m_posBox->setCurrentText(t->position);
        m_animBox->setCurrentText(t->animation);
        m_startSpin->setValue(t->start); m_durSpin->setValue(t->duration);
        m_sizeRow.set(t->sizePct);
        m_inspector->setCurrentIndex(2);
    } else {
        m_inspector->setCurrentIndex(0);
    }
    m_inspLock = false;
}

// ---- playback ----------------------------------------------------------------------------------------------

void MainWindow::setPlayhead(double t) { m_tl->setPlayhead(t); updateTimeUi(t); }

void MainWindow::updateTimeUi(double t) {
    const double dur = track().duration();
    m_previewStack->setCurrentIndex(track().clips().isEmpty() ? 0 : 1);
    m_curTime->setText(clock(t));
    m_totTime->setText(clock(dur));
    QSignalBlocker b(m_seek);
    m_seek->setRange(0, int(dur * 100));
    m_seek->setValue(int(t * 100));
}

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
    setPlayhead(t);
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
    setPlayhead(track().clipStart(m_currentClip) + std::max(0.0, src - c.sourceIn) / c.speed);
}

// ---- editing --------------------------------------------------------------------------------------------------

void MainWindow::afterEdit() {
    m_tl->refresh();
    if (track().clips().isEmpty()) { m_player->stop(); m_currentClip = -1; setPlayhead(0); return; }
    seek(std::min(m_tl->playhead(), std::max(0.0, track().duration() - 0.01)));
}

void MainWindow::splitAtPlayhead() {
    m_player->pause();
    Track trial = track();
    if (!trial.splitAt(m_tl->playhead())) {
        statusBar()->showMessage("Move the white line into the middle of a clip, then split.");
        return;
    }
    checkpoint("split");
    track() = trial;
    m_tl->clearSelection();
    afterEdit();
}

void MainWindow::removeSelected() {
    const int ti = m_tl->selectedText();
    if (ti >= 0) { checkpoint("del-text"); m_texts.remove(ti); m_tl->clearSelection(); m_tl->refresh(); return; }
    const int i = m_tl->selectedClip();
    if (i < 0) return;
    checkpoint("del-clip");
    m_player->pause();
    track().remove(i);
    m_tl->clearSelection();
    afterEdit();
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

    checkpoint("template");
    m_lastKey.clear();
    m_player->pause();
    track() = newTrack;
    m_texts = newTexts;
    m_settings = s;
    m_currentClip = -1;
    m_tl->clearSelection();
    m_tl->refresh();
    seek(0);
    statusBar()->showMessage("Template ready. Click any clip or title to change it.");
}

void MainWindow::exportVideo() {
    if (track().clips().isEmpty()) { statusBar()->showMessage("Add a video to the timeline first."); return; }
    if (!exportDialog(this, m_settings)) return;
    const QString out = QFileDialog::getSaveFileName(this, "Export", "output.mp4", "MP4 (*.mp4)");
    if (out.isEmpty()) return;
    m_bar->setValue(0);
    m_previewMode = false;
    statusBar()->showMessage("Exporting...");
    m_exporter.start(track(), m_texts, out, m_settings);
}

void MainWindow::quickPreview() {
    if (track().clips().isEmpty()) { statusBar()->showMessage("Add a video to the timeline first."); return; }
    ExportSettings s;
    s.base = 480; s.fps = 30; s.crf = 30; s.preset = "ultrafast"; s.upscale = "None";
    s.aspect = m_settings.aspect; s.fill = m_settings.fill;
    m_previewPath = QDir::temp().filePath("openclip_preview.mp4");
    m_bar->setValue(0);
    m_previewMode = true;
    statusBar()->showMessage("Making a quick preview...");
    m_exporter.start(track(), m_texts, m_previewPath, s);
}
