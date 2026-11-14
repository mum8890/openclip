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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>

namespace {

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

// A list whose items can be dragged onto the timeline (effects, transitions).
// No Q_OBJECT needed: it only overrides a virtual.
class DragList : public QListWidget {
public:
    explicit DragList(const QString& mime, QWidget* parent = nullptr)
        : QListWidget(parent), m_mime(mime) {
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setDefaultDropAction(Qt::CopyAction);
    }
protected:
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        if (items.isEmpty() || m_mime.isEmpty()) return QListWidget::mimeData(items);
        auto* md = new QMimeData;
        const QListWidgetItem* it = items.first();
        md->setData(m_mime, it->data(Qt::UserRole).toString().toUtf8());
        md->setText(it->text());
        return md;
    }
    // Always drag a copy, so the effect/transition list never loses its items.
    void startDrag(Qt::DropActions) override { QListWidget::startDrag(Qt::CopyAction); }
private:
    QString m_mime;
};

// Free / low-cost ways to run the assistant. Every entry is OpenAI-compatible.
struct AiPresetInfo {
    const char* key;
    const char* label;
    const char* url;
    const char* model;
    bool needsKey;
    const char* note;
};
const AiPresetInfo kAiPresets[] = {
    {"ollama", "Ollama - free, runs on this PC", "http://localhost:11434/v1", "llama3.2", false,
     "No account and no key. Install Ollama, then run: ollama pull llama3.2"},
    {"groq", "Groq - free API key", "https://api.groq.com/openai/v1", "llama-3.3-70b-versatile", true,
     "Free key from console.groq.com"},
    {"gemini", "Google Gemini - free API key", "https://generativelanguage.googleapis.com/v1beta/openai",
     "gemini-2.0-flash", true, "Free key from aistudio.google.com"},
    {"openrouter", "OpenRouter - free models", "https://openrouter.ai/api/v1",
     "meta-llama/llama-3.3-70b-instruct:free", true, "Free key from openrouter.ai/keys"},
    {"openai", "OpenAI - paid key", "https://api.openai.com/v1", "gpt-4o-mini", true,
     "Uses your own OpenAI credits."},
    {"custom", "Custom OpenAI-compatible server", "", "", false,
     "Point it at any server that speaks the OpenAI chat API."},
};

const AiPresetInfo* findAiPreset(const QString& key) {
    for (const auto& p : kAiPresets)
        if (key == QLatin1String(p.key)) return &p;
    return nullptr;
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
    m_agent = new AiAgent(this);
    m_bar = new QProgressBar;
    m_bar->setRange(0, 1000);
    m_bar->setFixedWidth(240);
    m_bar->setTextVisible(false);

    // live preview timers (a still frame after scrubbing, a proxy render after edits)
    m_fxTimer = new QTimer(this);
    m_fxTimer->setSingleShot(true);
    m_fxTimer->setInterval(1200);
    connect(m_fxTimer, &QTimer::timeout, this, &MainWindow::renderFxPreview);
    m_stillTimer = new QTimer(this);
    m_stillTimer->setSingleShot(true);
    m_stillTimer->setInterval(180);
    connect(m_stillTimer, &QTimer::timeout, this, &MainWindow::renderStill);
    // two files, so FFmpeg never writes into the file the player still has open
    m_proxyFiles[0] = QDir::temp().filePath("openclip_fx_a.mp4");
    m_proxyFiles[1] = QDir::temp().filePath("openclip_fx_b.mp4");
    QFile::remove(m_proxyFiles[0]);
    QFile::remove(m_proxyFiles[1]);

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
        buildListPanel("Drag a look onto any clip on the timeline (it appears as a purple FX block), "
                       "or click a look to apply it to the selected clip.",
                       kEffects, false, [this](const QString& s) { applyEffect(s); },
                       "application/x-openclip-effect"));
    m_dTransitions = makeDock("Transitions", "dock_transitions",
        buildListPanel("Drag a transition onto a cut between two clips (it appears as a block on the join, "
                       "drag its edge to change the length), or click a later clip then click a transition.",
                       kTransitions, true, [this](const QString& s) { applyTransition(s); },
                       "application/x-openclip-transition"));
    m_dTitles = makeDock("Titles", "dock_titles",
        buildListPanel("Move the white line to where you want the title, then click a style. Edit the words in Properties.",
                       kTextStyles, false, [this](const QString& s) { addTextWithStyle(s); }));
    m_dTemplates = makeDock("Templates", "dock_templates", buildTemplatesPanel());
    m_dProps = makeDock("Properties", "dock_props", buildInspector());
    m_dTimeline = makeDock("Timeline", "dock_timeline", buildTimelinePanel());
    m_dAi = makeDock("AI Assistant", "dock_ai", buildAiPanel());

    buildMenus();
    statusBar()->addPermanentWidget(m_bar);
    statusBar()->showMessage("Welcome! Open the Project panel, import a video, then double-click it to add it to the timeline.");

    connect(m_tl, &TimelineWidget::seekRequested, this, [this](double t) { seek(t); });
    connect(m_tl, &TimelineWidget::selectionChanged, this, &MainWindow::updateInspector);
    connect(m_tl, &TimelineWidget::zoomChanged, this, [this](double pps) { QSignalBlocker b(m_zoom); m_zoom->setValue(int(pps)); });

    // drag and drop from the Effects / Transitions panels
    connect(m_tl, &TimelineWidget::effectDropped, this, [this](const QString& name, int i) {
        if (i < 0 || i >= track().clips().size() || !kEffects.contains(name)) return;
        checkpoint("drop-effect");
        track().clips()[i].effect = name;
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        afterEdit();
        statusBar()->showMessage(QString("Dropped %1 on clip %2. It shows in the FX lane.").arg(name).arg(i + 1));
    });
    connect(m_tl, &TimelineWidget::transitionDropped, this, [this](const QString& name, int i) {
        if (i <= 0 || i >= track().clips().size() || !kTransitions.contains(name)) return;
        checkpoint("drop-transition");
        track().clips()[i].transition = name;
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        const double mid = track().clipStart(i) + track().joinOverlap(i) / 2.0;
        setPlayhead(mid);
        afterEdit();
        statusBar()->showMessage(QString("Dropped %1 on the join before clip %2. Drag the block edge to resize it.")
                                     .arg(transitionLabel(name)).arg(i + 1));
    });
    connect(m_tl, &TimelineWidget::removeEffectRequested, this, [this](int i) {
        if (i < 0 || i >= track().clips().size()) return;
        checkpoint("remove-effect");
        track().clips()[i].effect = "None";
        m_tl->refresh(); updateInspector(); markFxStale();
        statusBar()->showMessage("Effect removed.");
    });
    connect(m_tl, &TimelineWidget::removeTransitionRequested, this, [this](int i) {
        if (i <= 0 || i >= track().clips().size()) return;
        checkpoint("remove-transition");
        track().clips()[i].transition = "None";
        m_tl->refresh(); updateInspector(); markFxStale();
        statusBar()->showMessage("Transition removed. The clips now cut straight.");
    });
    connect(m_tl, &TimelineWidget::transitionEditStarted, this, [this](int) {
        checkpoint("transition-length");          // remember the length before the drag
    });
    connect(m_tl, &TimelineWidget::transitionEditFinished, this, [this] {
        m_tl->refresh();
        updateInspector();
        markFxStale();
    });

    // AI assistant
    connect(m_agent, &AiAgent::assistantText, this, [this](const QString& t) { appendChat("OpenClip AI", t, false); });
    connect(m_agent, &AiAgent::status, this, [this](const QString& s) { if (m_aiStatus) m_aiStatus->setText(s); });
    connect(m_agent, &AiAgent::failed, this, [this](const QString& s) {
        appendChat("OpenClip AI", s, false);
        if (m_aiStatus) m_aiStatus->setText("Ready");
    });
    connect(m_agent, &AiAgent::finished, this, [this] {
        if (m_aiSend) m_aiSend->setEnabled(true);
        if (m_aiStatus) m_aiStatus->setText("Ready");
        if (m_aiInput) m_aiInput->setFocus();
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, &MainWindow::onPosition);
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        m_playBtn->setIcon(glyphIcon(s == QMediaPlayer::PlayingState));
        updatePreviewPage();
        if (s != QMediaPlayer::PlayingState) requestStillPreview();
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus s) {
        if (s == QMediaPlayer::LoadedMedia || s == QMediaPlayer::BufferedMedia) applyPending();
    });
    connect(&m_exporter, &Exporter::progress, this, [this](double f) {
        m_bar->setValue(int(f * 1000));
        if (m_exportDlg) m_exportDlg->setProgress(f);
    });
    connect(&m_exporter, &Exporter::finished, this, [this](bool ok, const QString& log) {
        const bool wasPreview = m_previewMode;
        const bool cancelled = log.startsWith("Cancelled");
        m_previewMode = false;
        m_exporting = false;
        closeProgressDialog(ok, ok ? (wasPreview ? "Preview ready." : "Export finished.")
                                   : (cancelled ? "Cancelled." : "Export failed."));
        if (!ok) {
            if (!cancelled) QMessageBox::warning(this, "Export failed", log.right(1500));
            statusBar()->showMessage(cancelled ? "Export cancelled." : "Export failed.");
            m_bar->setValue(0);
            return;
        }
        m_bar->setValue(1000);
        if (wasPreview) { statusBar()->showMessage("Preview ready."); QDesktopServices::openUrl(QUrl::fromLocalFile(m_previewPath)); }
        else { statusBar()->showMessage("Export finished."); QMessageBox::information(this, "Export", "Done!"); }
    });

    // the low-res proxy behind "FX preview": rendered quietly in the background
    connect(&m_fx, &Exporter::progress, this, [this](double f) {
        if (m_fxBadge) m_fxBadge->setText(QString("FX %1%").arg(int(f * 100.0)));
        m_bar->setValue(int(f * 1000));
    });
    connect(&m_fx, &Exporter::finished, this, [this](bool ok, const QString&) {
        m_bar->setValue(0);
        if (!ok) {
            if (m_fxBadge) m_fxBadge->setText(m_fxEnabled ? "FX waiting" : "");
            if (m_proxyStale) m_fxTimer->start();
            return;
        }
        if (m_fxJobGen != m_fxGen) { m_fxTimer->start(); return; }   // edited while it was rendering
        m_proxyPath = m_fxJobPath;
        m_proxyValid = QFileInfo(m_proxyPath).size() > 1000;
        m_proxyStale = false;
        if (m_fxBadge) m_fxBadge->setText(m_proxyValid ? "FX ready" : "");
        if (m_playWhenReady && m_player->playbackState() == QMediaPlayer::PlayingState) startProxyPlayback();
        m_playWhenReady = false;
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
    const QList<QDockWidget*> all{m_dProject, m_dEffects, m_dTransitions, m_dTitles, m_dTemplates, m_dProps, m_dTimeline, m_dAi};
    for (auto* d : all) { d->setFloating(false); removeDockWidget(d); }
    addDockWidget(Qt::LeftDockWidgetArea, m_dProject);
    for (auto* d : {m_dEffects, m_dTransitions, m_dTitles, m_dTemplates}) {
        addDockWidget(Qt::LeftDockWidgetArea, d);
        tabifyDockWidget(m_dProject, d);
    }
    addDockWidget(Qt::RightDockWidgetArea, m_dProps);
    addDockWidget(Qt::RightDockWidgetArea, m_dAi);
    tabifyDockWidget(m_dProps, m_dAi);
    addDockWidget(Qt::BottomDockWidgetArea, m_dTimeline);
    for (auto* d : all) d->show();

    QDockWidget* front = m_dProject;
    int left = 380, right = 320, bottom = 300;
    if (name == "Assembly") { left = 560; bottom = 340; }
    else if (name == "Effects") front = m_dEffects;
    else if (name == "Titles") front = m_dTitles;
    else if (name == "AI") front = m_dAi;
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
    for (const QString& t : kTransitions) tr->addAction(transitionLabel(t), this, [this, t] { applyTransition(t); });
    auto* sp = clip->addMenu("Speed");
    for (double s : {0.5, 0.75, 1.0, 1.25, 1.5, 2.0}) sp->addAction(QString("%1x").arg(s), this, [this, s] { applySpeed(s); });

    auto* seq = mb->addMenu("Sequence");
    seq->addAction("Play / Pause", this, &MainWindow::togglePlay)->setShortcut(QKeySequence(Qt::Key_Space));
    seq->addAction("Go to Start", this, [this] { if (!track().clips().isEmpty()) seek(0); })->setShortcut(QKeySequence(Qt::Key_Home));
    seq->addAction("Go to End", this, [this] { if (!track().clips().isEmpty()) seek(std::max(0.0, track().duration() - 0.05)); })->setShortcut(QKeySequence(Qt::Key_End));

    auto* gfx = mb->addMenu("Graphics and Titles");
    auto* addT = gfx->addMenu("Add Title");
    for (const QString& s : kTextStyles) addT->addAction(s, this, [this, s] { addTextWithStyle(s); });

    auto* aiMenu = mb->addMenu("AI");
    aiMenu->addAction("Ask the AI Assistant", this, [this] {
        if (m_dAi) { m_dAi->show(); m_dAi->raise(); }
        if (m_aiInput) m_aiInput->setFocus();
    })->setShortcut(QKeySequence("Ctrl+Shift+A"));
    aiMenu->addAction("New chat", this, &MainWindow::newAiChat);

    auto* view = mb->addMenu("View");
    view->addAction("Zoom In Timeline", this, [this] { m_zoom->setValue(int(m_zoom->value() * 1.25) + 1); })->setShortcut(QKeySequence("Ctrl+="));
    view->addAction("Zoom Out Timeline", this, [this] { m_zoom->setValue(int(m_zoom->value() / 1.25)); })->setShortcut(QKeySequence("Ctrl+-"));
    view->addSeparator();
    m_fxAction = view->addAction("FX preview (live effects and transitions)");
    m_fxAction->setCheckable(true);
    m_fxAction->setChecked(true);
    connect(m_fxAction, &QAction::toggled, this, &MainWindow::setFxEnabled);

    auto* win = mb->addMenu("Window");
    auto* ws = win->addMenu("Workspaces");
    for (const QString& n : {"Editing", "Assembly", "Effects", "Titles", "AI"})
        ws->addAction(n, this, [this, n] { applyWorkspace(n, true); });
    win->addAction("Reset to Default Layout", this, [this] { applyWorkspace("Editing", true); });
    win->addSeparator();
    for (auto* d : {m_dProject, m_dEffects, m_dTransitions, m_dTitles, m_dTemplates, m_dProps, m_dTimeline, m_dAi})
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
    // page 2: the frame FFmpeg just rendered for this exact moment (looks, transitions, titles)
    m_stillView = new QLabel;
    m_stillView->setAlignment(Qt::AlignCenter);
    m_stillView->setMinimumSize(320, 180);
    m_stillView->setStyleSheet("background:#000;");
    m_previewStack->addWidget(m_stillView);
    v->addWidget(m_previewStack, 1);

    auto* bar = new QWidget; bar->setObjectName("playerbar");
    auto* l = new QHBoxLayout(bar);
    l->setContentsMargins(12, 8, 14, 8); l->setSpacing(12);
    m_playBtn = new QPushButton; m_playBtn->setObjectName("round");
    m_playBtn->setIcon(glyphIcon(false)); m_playBtn->setIconSize(QSize(20, 20));
    m_playBtn->setToolTip("Play / Pause (Space) - plays the rendered preview when it is ready");
    m_curTime = new QLabel("00:00.0"); m_totTime = new QLabel("00:00.0"); m_totTime->setObjectName("muted");
    m_seek = new QSlider(Qt::Horizontal);
    m_fxToggle = new QPushButton("FX preview");
    m_fxToggle->setCheckable(true);
    m_fxToggle->setChecked(true);
    m_fxToggle->setToolTip("Show the real effect, transition and title under the playhead, and play the "
                           "rendered preview instead of the raw clips. Turn it off on a slow computer.");
    m_fxBadge = new QLabel;
    m_fxBadge->setObjectName("muted");
    m_fxBadge->setMinimumWidth(70);
    l->addWidget(m_playBtn); l->addWidget(m_curTime); l->addWidget(m_seek, 1);
    l->addWidget(m_fxToggle); l->addWidget(m_fxBadge); l->addWidget(m_totTime);
    v->addWidget(bar);
    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::togglePlay);
    connect(m_seek, &QSlider::sliderMoved, this, [this](int v) { seek(v / 100.0); });
    connect(m_fxToggle, &QPushButton::toggled, this, &MainWindow::setFxEnabled);
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
                                    std::function<void(const QString&)> onClick,
                                    const QString& dragMime) {
    auto* page = new QWidget; page->setObjectName("panel");
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(10, 10, 10, 10); v->setSpacing(8);
    auto* h = new QLabel(hint); h->setObjectName("hint"); h->setWordWrap(true);
    QListWidget* list = dragMime.isEmpty() ? new QListWidget : new DragList(dragMime);
    for (const QString& n : items) {
        auto* it = new QListWidgetItem(transitions ? transitionLabel(n) : n);
        it->setData(Qt::UserRole, n);
        if (!dragMime.isEmpty()) {
            it->setToolTip(transitions
                ? QString("Drag onto a cut in the timeline, or click to apply %1 to the selected clip.").arg(transitionLabel(n))
                : QString("Drag onto a clip in the timeline, or click to apply %1 to the selected clip.").arg(n));
        }
        list->addItem(it);
    }
    v->addWidget(h); v->addWidget(list, 1);
    connect(list, &QListWidget::itemClicked, this, [onClick](QListWidgetItem* it) { onClick(it->data(Qt::UserRole).toString()); });
    return page;
}

QWidget* MainWindow::buildAiPanel() {
    auto* page = new QWidget; page->setObjectName("panel");
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(10, 10, 10, 10); v->setSpacing(8);

    auto* intro = new QLabel("Ask in plain words. The assistant can add clips, looks, transitions, titles, "
                             "speed and colour changes, cuts and previews - right on your timeline.");
    intro->setObjectName("hint"); intro->setWordWrap(true);

    m_aiPreset = new QComboBox;
    for (const auto& p : kAiPresets) m_aiPreset->addItem(tr(p.label), QString::fromLatin1(p.key));
    m_aiUrl = new QLineEdit;   m_aiUrl->setPlaceholderText("Base URL (usually ends with /v1)");
    m_aiModel = new QLineEdit; m_aiModel->setPlaceholderText("Model name");
    m_aiKey = new QLineEdit;   m_aiKey->setEchoMode(QLineEdit::Password);
    m_aiKey->setPlaceholderText("API key - leave empty for Ollama");
    m_aiAsk = new QCheckBox("Ask me before changing the timeline");
    auto* save = new QPushButton("Save AI settings");
    auto* note = new QLabel("Free options: Ollama runs on this PC with no key; Groq, Gemini and OpenRouter give free keys. "
                            "The key is stored only on this PC. Only timeline details (file names, effects) are sent - never your videos.");
    note->setObjectName("hint"); note->setWordWrap(true);

    auto* provider = new QWidget; provider->setObjectName("glassbox");
    auto* pv = new QVBoxLayout(provider);
    pv->setContentsMargins(9, 9, 9, 9); pv->setSpacing(6);
    pv->addWidget(m_aiPreset); pv->addWidget(m_aiUrl); pv->addWidget(m_aiModel); pv->addWidget(m_aiKey);
    pv->addWidget(m_aiAsk); pv->addWidget(save); pv->addWidget(note);

    m_chat = new QTextBrowser;
    m_chat->setObjectName("chat");
    m_chat->setOpenExternalLinks(true);
    m_chat->setMinimumHeight(180);
    m_chat->setPlaceholderText("Your conversation with the assistant appears here.");

    m_aiStatus = new QLabel("Ready"); m_aiStatus->setObjectName("muted");
    m_aiInput = new QLineEdit;
    m_aiInput->setPlaceholderText("Ask OpenClip AI...");
    m_aiSend = new QPushButton("Send"); m_aiSend->setObjectName("accent");
    auto* fresh = new QPushButton("New chat");

    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->addWidget(m_aiInput, 1); row->addWidget(m_aiSend); row->addWidget(fresh);

    v->addWidget(intro);
    v->addWidget(provider);
    v->addWidget(m_chat, 1);
    v->addWidget(m_aiStatus);
    v->addLayout(row);

    QSettings st("OpenClip", "OpenClip");
    const QString presetKey = st.value("ai/preset", "ollama").toString();
    int idx = m_aiPreset->findData(presetKey);
    if (idx < 0) idx = 0;
    {
        QSignalBlocker b(m_aiPreset);
        m_aiPreset->setCurrentIndex(idx);
    }
    const AiPresetInfo* preset = findAiPreset(presetKey);
    m_aiUrl->setText(st.value("ai/url", preset ? QString::fromLatin1(preset->url) : QString()).toString());
    m_aiModel->setText(st.value("ai/model", preset ? QString::fromLatin1(preset->model) : QString()).toString());
    m_aiKey->setText(st.value("ai/key/" + presetKey).toString());
    m_aiAsk->setChecked(st.value("ai/ask", true).toBool());

    connect(m_aiPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::applyAiPreset);
    connect(save, &QPushButton::clicked, this, &MainWindow::saveAiSettings);
    connect(m_aiSend, &QPushButton::clicked, this, &MainWindow::sendAiMessage);
    connect(m_aiInput, &QLineEdit::returnPressed, this, &MainWindow::sendAiMessage);
    connect(fresh, &QPushButton::clicked, this, &MainWindow::newAiChat);

    appendChat("OpenClip AI", "Hi! Tell me how you want the video to feel, for example: "
                              "\"make clip 2 black and white and add a slow fade\", or "
                              "\"put a title at the start for 3 seconds\".", false);
    return page;
}

QWidget* MainWindow::buildTemplatesPanel() {
    auto* tp = new QWidget; tp->setObjectName("panel");
    auto* tv = new QVBoxLayout(tp);
    tv->setContentsMargins(10, 10, 10, 10); tv->setSpacing(10);
    auto* th = new QLabel("Start from a ready-made edit: pick a template, choose a video for each part, and OpenClip builds the project with effects, transitions and titles already set.");
    th->setObjectName("hint"); th->setWordWrap(true);
    auto* tb = new QPushButton("Browse templates..."); tb->setObjectName("accent");
    // \u escapes keep this file pure ASCII, so the bullet never depends on the build code page.
    auto* names = new QLabel(QStringLiteral(
        "\u2022 Gameplay Highlight\n"
        "\u2022 Vertical Short\n"
        "\u2022 Kill Montage\n"
        "\u2022 Cinematic Trailer"));
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
    for (const QString& t : kTransitions) m_transBox->addItem(transitionLabel(t), t);
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
    m_playProxy = false; m_proxyValid = false; m_proxyStale = true;
    if (m_fx.isRunning()) m_fx.cancel();
    m_fxTimer->stop();
    if (m_stillProc) { m_stillProc->kill(); m_stillProc->deleteLater(); m_stillProc = nullptr; }
    if (m_fxBadge) m_fxBadge->setText(m_fxEnabled ? "" : "FX off");
    m_stillTime = -1; m_stillPix = QPixmap();
    m_tl->clearSelection();
    m_tl->refresh();
    setPlayhead(0);
    updatePreviewPage();
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
    m_playProxy = false; m_proxyValid = false; m_proxyStale = true;
    if (m_stillProc) { m_stillProc->kill(); m_stillProc->deleteLater(); m_stillProc = nullptr; }
    m_stillTime = -1; m_stillPix = QPixmap();
    m_tl->clearSelection();
    m_tl->refresh();
    if (!track().clips().isEmpty()) seek(0); else setPlayhead(0);
    markFxStale();
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
    if (m_stillProc) { m_stillProc->kill(); m_stillProc->waitForFinished(1000); }
    if (m_fx.isRunning()) m_fx.cancel();
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
    markFxStale();          // the preview no longer matches the timeline
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
        statusBar()->showMessage("Effect: " + name + ". The preview shows the real look.");
    } else statusBar()->showMessage("Click a clip on the timeline first.");
}

void MainWindow::applyTransition(const QString& name) {
    const int i = m_tl->selectedClip();
    if (i < 0) { statusBar()->showMessage("Click a clip on the timeline first."); return; }
    if (i == 0) { statusBar()->showMessage("The first clip has nothing before it. Click a later clip."); return; }
    checkpoint("transition");
    track().clips()[i].transition = name;
    m_tl->refresh(); updateInspector();
    statusBar()->showMessage("Transition: " + transitionLabel(name) + ". The preview shows it too.");
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

// ---- AI assistant ---------------------------------------------------------------------------------------

AiSettings MainWindow::aiSettings() const {
    AiSettings s;
    if (m_aiPreset) s.preset = m_aiPreset->currentData().toString();
    if (m_aiUrl) s.baseUrl = m_aiUrl->text().trimmed();
    if (m_aiModel) s.model = m_aiModel->text().trimmed();
    if (m_aiKey) s.apiKey = m_aiKey->text().trimmed();
    if (m_aiAsk) s.askBefore = m_aiAsk->isChecked();
    return s;
}

void MainWindow::saveAiSettings() {
    if (!m_aiPreset || !m_aiUrl || !m_aiModel || !m_aiKey || !m_aiAsk) return;
    const QString presetKey = m_aiPreset->currentData().toString();
    QSettings st("OpenClip", "OpenClip");
    st.setValue("ai/preset", presetKey);
    st.setValue("ai/url", m_aiUrl->text().trimmed());
    st.setValue("ai/model", m_aiModel->text().trimmed());
    st.setValue("ai/ask", m_aiAsk->isChecked());
    st.setValue("ai/key/" + presetKey, m_aiKey->text().trimmed());
    if (m_aiStatus) m_aiStatus->setText("Settings saved");
    statusBar()->showMessage("AI settings saved on this PC.");
}

void MainWindow::applyAiPreset(int index) {
    if (index < 0 || !m_aiPreset || !m_aiUrl || !m_aiModel || !m_aiKey) return;
    const QString key = m_aiPreset->itemData(index).toString();
    const AiPresetInfo* p = findAiPreset(key);
    if (!p) return;
    if (key != "custom") {
        m_aiUrl->setText(QString::fromLatin1(p->url));
        m_aiModel->setText(QString::fromLatin1(p->model));
    }
    QSettings st("OpenClip", "OpenClip");
    m_aiKey->setText(st.value("ai/key/" + key).toString());
    if (m_aiStatus) m_aiStatus->setText(p->needsKey ? "Needs a free API key" : "Ready - no key needed");
    statusBar()->showMessage(QString::fromLatin1(p->note));
}

void MainWindow::appendChat(const QString& who, const QString& text, bool user) {
    if (!m_chat) return;
    QString html = text.toHtmlEscaped();
    html.replace('\n', "<br>");
    m_chat->append(QString("<div style=\"margin:6px 2px\">"
                           "<span style=\"color:%1;font-weight:bold\">%2</span><br>%3</div>")
                       .arg(user ? QStringLiteral("#67E8F9") : QStringLiteral("#C4B5FD"),
                            who.toHtmlEscaped(), html));
    if (auto* bar = m_chat->verticalScrollBar()) bar->setValue(bar->maximum());
}

void MainWindow::newAiChat() {
    if (m_agent) m_agent->reset();
    if (m_chat) m_chat->clear();
    if (m_aiStatus) m_aiStatus->setText("Ready");
    if (m_aiSend) m_aiSend->setEnabled(true);
    appendChat("OpenClip AI", "New chat. What should we edit?", false);
    if (m_dAi) { m_dAi->show(); m_dAi->raise(); }
    if (m_aiInput) m_aiInput->setFocus();
}

QString MainWindow::aiSystemPrompt() const {
    return QStringLiteral(
        "You are the editing assistant inside OpenClip, a free Windows video editor. "
        "You change the user's timeline only through the tools you have been given.\n"
        "Rules:\n"
        "- Call get_timeline before changing anything, and use the 0-based indexes it returns.\n"
        "- Times are in seconds. Keep replies short, warm and clear (1-3 sentences).\n"
        "- Prefer one clear change per tool call. Never invent file names or clip indexes.\n"
        "- If a tool cannot do what the user asks, say so honestly instead of pretending.\n"
        "- Do not say you exported, rendered or uploaded anything: the user presses Export for that.");
}

QString MainWindow::describeAiCall(const QString& name, const QJsonObject& args) const {
    if (name == "get_timeline") return "Read the timeline and media library";
    if (name == "add_clip")     return QString("Add media item %1 to the timeline").arg(args.value("media").toInt());
    if (name == "set_effect")   return QString("Set the look \"%1\" on clip %2").arg(args.value("effect").toString()).arg(args.value("clip").toInt() + 1);
    if (name == "set_transition") return QString("Add the %1 transition before clip %2").arg(args.value("transition").toString()).arg(args.value("clip").toInt() + 1);
    if (name == "set_speed")    return QString("Set clip %1 speed to %2x").arg(args.value("clip").toInt() + 1).arg(args.value("speed").toDouble(), 0, 'f', 2);
    if (name == "set_colour")   return QString("Adjust colour on clip %1").arg(args.value("clip").toInt() + 1);
    if (name == "add_title")    return QString("Add the title \"%1\"").arg(args.value("text").toString());
    if (name == "split_at")     return QString("Split the clip at %1 s").arg(args.value("time").toDouble(), 0, 'f', 1);
    if (name == "remove_clip")  return QString("Delete clip %1").arg(args.value("clip").toInt() + 1);
    if (name == "seek")         return QString("Move the playhead to %1 s").arg(args.value("time").toDouble(), 0, 'f', 1);
    return "Run " + name;
}

bool MainWindow::approveAiTools(const QJsonArray& calls) {
    if (!m_aiAsk || !m_aiAsk->isChecked()) return true;
    QStringList lines;
    for (const QJsonValue& v : calls) {
        const QJsonObject call = v.toObject();
        const QJsonObject fn = call.value("function").toObject();
        QJsonObject args;
        const QJsonValue raw = fn.value("arguments");
        if (raw.isObject()) args = raw.toObject();
        else if (raw.isString()) {
            const QJsonDocument d = QJsonDocument::fromJson(raw.toString().toUtf8());
            if (d.isObject()) args = d.object();
        }
        lines << QStringLiteral("\u2022  %1").arg(describeAiCall(fn.value("name").toString(), args));
    }
    const auto r = QMessageBox::question(this, "Apply AI changes",
        "The assistant wants to:\n\n" + lines.join("\n") + "\n\nApply these changes?\n"
        "(You can turn this question off in the AI panel, and undo with Ctrl+Z.)",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    return r == QMessageBox::Yes;
}

QJsonObject MainWindow::timelineSnapshot() {
    QJsonObject out;
    out["duration"] = track().duration();
    out["playhead"] = m_tl ? m_tl->playhead() : 0.0;

    QJsonArray clips;
    for (int i = 0; i < track().clips().size(); ++i) {
        const Clip& c = track().clips()[i];
        QJsonObject j;
        j["index"] = i;
        j["file"] = QFileInfo(c.path).fileName();
        j["start"] = track().clipStart(i);
        j["duration"] = c.duration();
        j["speed"] = c.speed;
        j["effect"] = c.effect;
        j["transition"] = c.transition;
        j["transitionDuration"] = track().joinOverlap(i);
        clips.append(j);
    }
    out["clips"] = clips;

    QJsonArray titles;
    for (int i = 0; i < m_texts.size(); ++i) {
        const TextItem& t = m_texts[i];
        QJsonObject j;
        j["index"] = i;
        j["text"] = t.text;
        j["start"] = t.start;
        j["duration"] = t.duration;
        j["style"] = t.style;
        j["position"] = t.position;
        titles.append(j);
    }
    out["titles"] = titles;

    QJsonArray media;
    if (m_media) {
        for (int i = 0; i < m_media->count(); ++i) {
            const QListWidgetItem* it = m_media->item(i);
            QJsonObject j;
            j["index"] = i;
            j["file"] = QFileInfo(it->data(Qt::UserRole).toString()).fileName();
            j["duration"] = it->data(Qt::UserRole + 1).toDouble();
            media.append(j);
        }
    }
    out["media"] = media;
    return out;
}

QJsonObject MainWindow::runAiTool(const QString& name, const QJsonObject& args) {
    auto fail = [](const QString& msg) {
        QJsonObject o; o["ok"] = false; o["error"] = msg; return o;
    };
    auto done = [] { QJsonObject o; o["ok"] = true; return o; };

    if (name == "get_timeline") {
        QJsonObject o = done();
        o["timeline"] = timelineSnapshot();
        o["hint"] = QStringLiteral("Clip and media indexes are 0-based. Times are seconds.");
        return o;
    }

    if (name == "seek") {
        const double t = std::clamp(args.value("time").toDouble(0.0), 0.0,
                                    std::max(0.0, track().duration()));
        if (!track().clips().isEmpty()) seek(t);
        QJsonObject o = done(); o["time"] = t; return o;
    }

    if (name == "add_clip") {
        const int idx = args.value("media").toInt(-1);
        if (!m_media || idx < 0 || idx >= m_media->count())
            return fail(QString("There is no media item %1. Call get_timeline to see the media library (0-based).").arg(idx));
        if (m_media->item(idx)->data(Qt::UserRole + 1).toDouble() <= 0)
            return fail("That media item has no readable duration.");
        addMediaItem(m_media->item(idx));
        QJsonObject o = done();
        o["clip"] = track().clips().size() - 1;
        o["file"] = QFileInfo(track().clips().last().path).fileName();
        return o;
    }

    if (name == "set_effect") {
        const int i = args.value("clip").toInt(-1);
        if (i < 0 || i >= track().clips().size())
            return fail(QString("There is no clip %1. Call get_timeline first.").arg(i));
        const QString e = args.value("effect").toString();
        if (!kEffects.contains(e))
            return fail("Unknown effect \"" + e + "\". Use one of: " + kEffects.join(", "));
        checkpoint("ai-effect");
        track().clips()[i].effect = e;
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        markFxStale();
        QJsonObject o = done(); o["clip"] = i; o["effect"] = e; return o;
    }

    if (name == "set_transition") {
        const int i = args.value("clip").toInt(-1);
        if (i <= 0 || i >= track().clips().size())
            return fail(QString("Clip %1 cannot take a transition: the index must be 1 or more, and less than the clip count %2.")
                            .arg(i).arg(track().clips().size()));
        const QString t = args.value("transition").toString();
        if (!kTransitions.contains(t))
            return fail("Unknown transition \"" + t + "\". Use an FFmpeg xfade name such as fade, dissolve, slideleft, circleopen.");
        checkpoint("ai-transition");
        Clip& cur = track().clips()[i];
        cur.transition = t;
        if (args.contains("duration") && args.value("duration").toDouble() > 0) {
            const double maxDur = std::max(0.1, std::min(2.0, 0.5 * std::min(track().clips()[i - 1].duration(), cur.duration())));
            cur.transitionDur = std::clamp(args.value("duration").toDouble(0.5), 0.1, maxDur);
        }
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        const double mid = track().clipStart(i) + track().joinOverlap(i) / 2.0;
        setPlayhead(mid);
        markFxStale();
        QJsonObject o = done(); o["clip"] = i; o["transition"] = t; o["duration"] = cur.transitionDur; return o;
    }

    if (name == "set_speed") {
        const int i = args.value("clip").toInt(-1);
        if (i < 0 || i >= track().clips().size())
            return fail(QString("There is no clip %1. Call get_timeline first.").arg(i));
        const double sp = std::clamp(args.value("speed").toDouble(1.0), 0.5, 2.0);
        checkpoint("ai-speed");
        track().clips()[i].speed = sp;
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        afterEdit();
        QJsonObject o = done(); o["clip"] = i; o["speed"] = sp; return o;
    }

    if (name == "set_colour") {
        const int i = args.value("clip").toInt(-1);
        if (i < 0 || i >= track().clips().size())
            return fail(QString("There is no clip %1. Call get_timeline first.").arg(i));
        checkpoint("ai-colour");
        Clip& c = track().clips()[i];
        if (args.contains("brightness")) c.brightness = std::clamp(args.value("brightness").toDouble(0.0), -1.0, 1.0);
        if (args.contains("contrast"))   c.contrast   = std::clamp(args.value("contrast").toDouble(1.0), 0.0, 3.0);
        if (args.contains("saturation")) c.saturation = std::clamp(args.value("saturation").toDouble(1.0), 0.0, 3.0);
        m_tl->selectClip(i);
        m_tl->refresh();
        updateInspector();
        markFxStale();
        QJsonObject o = done();
        o["brightness"] = c.brightness; o["contrast"] = c.contrast; o["saturation"] = c.saturation;
        return o;
    }

    if (name == "add_title") {
        const QString text = args.value("text").toString().trimmed();
        if (text.isEmpty()) return fail("The title text is empty.");
        QString style = args.value("style").toString();
        if (!kTextStyles.contains(style)) style = "Gaming";
        QString pos = args.value("position").toString();
        if (!kTextPositions.contains(pos)) pos = "Bottom";
        TextItem t;
        t.text = text;
        t.start = std::max(0.0, args.value("start").toDouble(m_tl->playhead()));
        t.duration = std::clamp(args.value("duration").toDouble(3.0), 0.5, 600.0);
        t.style = style;
        t.position = pos;
        checkpoint("ai-title");
        m_texts.append(t);
        m_tl->refresh();
        m_tl->selectText(m_texts.size() - 1);
        markFxStale();
        QJsonObject o = done(); o["title"] = m_texts.size() - 1; o["text"] = text; return o;
    }

    if (name == "split_at") {
        m_player->pause();
        Track trial = track();
        const double at = args.value("time").toDouble(m_tl->playhead());
        if (!trial.splitAt(at))
            return fail(QString("No clip can be cut at %1 s. The time must be inside a clip and not inside a transition.").arg(at, 0, 'f', 2));
        checkpoint("ai-split");
        track() = trial;
        m_tl->clearSelection();
        afterEdit();
        QJsonObject o = done(); o["time"] = at; return o;
    }

    if (name == "remove_clip") {
        const int i = args.value("clip").toInt(-1);
        if (i < 0 || i >= track().clips().size())
            return fail(QString("There is no clip %1. Call get_timeline first.").arg(i));
        checkpoint("ai-remove");
        m_player->pause();
        track().remove(i);
        m_tl->clearSelection();
        afterEdit();
        QJsonObject o = done(); o["removed"] = i; return o;
    }

    return fail("Unknown tool \"" + name + "\".");
}

void MainWindow::sendAiMessage() {
    if (!m_agent || !m_aiInput) return;
    if (m_agent->busy()) return;
    const QString text = m_aiInput->text().trimmed();
    if (text.isEmpty()) return;

    AiSettings s = aiSettings();
    if (s.baseUrl.isEmpty()) {
        statusBar()->showMessage("Choose a provider or type a Base URL first.");
        appendChat("OpenClip AI", "I need a provider first. Pick one in the list above (Ollama is free and needs no key).", false);
        return;
    }
    if (s.model.isEmpty()) {
        appendChat("OpenClip AI", "The model name is empty. Put one in the AI settings above (for example llama3.2).", false);
        return;
    }
    const AiPresetInfo* p = findAiPreset(s.preset);
    if (p && p->needsKey && s.apiKey.isEmpty()) {
        appendChat("OpenClip AI", QString("This provider needs a free API key. %1").arg(QString::fromLatin1(p->note)), false);
        statusBar()->showMessage("Paste an API key in the AI panel first.");
        return;
    }

    saveAiSettings();
    appendChat("You", text, true);
    m_aiInput->clear();
    m_aiSend->setEnabled(false);
    if (m_aiStatus) m_aiStatus->setText("Thinking...");
    m_agent->start(s, aiSystemPrompt(), text,
        [this](const QString& n, const QJsonObject& a) { return runAiTool(n, a); },
        [this](const QJsonArray& c) { return approveAiTools(c); });
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

void MainWindow::setPlayhead(double t) {
    m_tl->setPlayhead(t);
    updateTimeUi(t);
    if (m_player->playbackState() != QMediaPlayer::PlayingState) requestStillPreview();
}

void MainWindow::updateTimeUi(double t) {
    const double dur = track().duration();
    m_curTime->setText(clock(t));
    m_totTime->setText(clock(dur));
    QSignalBlocker b(m_seek);
    m_seek->setRange(0, int(dur * 100));
    m_seek->setValue(int(t * 100));
    updatePreviewPage();
}

// Which picture the middle of the window shows: placeholder, live video, or the still frame.
void MainWindow::updatePreviewPage() {
    if (track().clips().isEmpty()) { m_previewStack->setCurrentIndex(0); return; }
    const bool playing = m_player->playbackState() == QMediaPlayer::PlayingState;
    const bool fresh = m_fxEnabled && !m_stillPix.isNull() &&
                       std::abs(m_stillTime - m_tl->playhead()) < 0.2;
    m_previewStack->setCurrentIndex((!playing && fresh) ? 2 : 1);
}

void MainWindow::togglePlay() {
    if (track().clips().isEmpty()) return;
    if (m_playProxy) {                       // already on the rendered preview
        if (m_player->playbackState() == QMediaPlayer::PlayingState) m_player->pause();
        else if (m_tl->playhead() >= track().duration() - 0.05) seek(0, true);
        else m_player->play();
        return;
    }
    if (m_player->playbackState() == QMediaPlayer::PlayingState) { m_player->pause(); return; }
    if (m_tl->playhead() >= track().duration() - 0.05) setPlayhead(0);

    if (m_fxEnabled && m_proxyValid && !m_proxyStale && QFileInfo::exists(m_proxyPath)) {
        startProxyPlayback();
        return;
    }
    playRawAt(m_tl->playhead(), true);        // never make the user wait: play now...
    if (m_fxEnabled) {
        m_playWhenReady = true;               // ...then switch to the real render when it is ready
        renderFxPreview();
    }
}

void MainWindow::playRawAt(double t, bool play) {
    m_playProxy = false;
    seek(t, play);
}

void MainWindow::seek(double t, bool play) {
    if (track().clips().isEmpty()) return;
    t = std::clamp(t, 0.0, std::max(0.0, track().duration() - 0.001));

    if (m_playProxy) {                        // the rendered preview has the same clock
        setPlayhead(t);
        m_pendingSeekMs = qint64(t * 1000.0);
        m_playAfterLoad = play;
        applyPending();                       // the proxy is already loaded
        return;
    }

    const int i = track().clipAt(t);
    if (i < 0) return;
    const Clip& c = track().clips()[i];
    const double raw = c.sourceIn + (t - track().clipStart(i)) * c.speed;
    const double src = std::clamp(raw, c.sourceIn, std::max(c.sourceIn, c.sourceOut - 0.02));
    setPlayhead(t);
    loadClip(i, src, play);
}

void MainWindow::loadClip(int index, double sourceTime, bool play) {
    if (index < 0 || index >= track().clips().size()) return;
    const Clip& c = track().clips()[index];
    const QUrl url = QUrl::fromLocalFile(c.path);
    m_currentClip = index;
    m_pendingSeekMs = qint64(sourceTime * 1000);
    m_playAfterLoad = play;
    if (m_player->source() != url) m_player->setSource(url);
    else applyPending();
}

void MainWindow::applyPending() {
    if (m_pendingSeekMs < 0) return;
    if (m_playProxy) {
        m_player->setPlaybackRate(1.0);
        m_player->setPosition(std::clamp<qint64>(m_pendingSeekMs, 0, qint64(track().duration() * 1000.0)));
        m_pendingSeekMs = -1;
        if (m_playAfterLoad) m_player->play(); else m_player->pause();
        return;
    }
    if (m_currentClip < 0 || m_currentClip >= track().clips().size()) return;
    m_player->setPlaybackRate(track().clips()[m_currentClip].speed);
    m_player->setPosition(m_pendingSeekMs);
    m_pendingSeekMs = -1;
    if (m_playAfterLoad) m_player->play(); else m_player->pause();
}

void MainWindow::onPosition(qint64 ms) {
    if (m_pendingSeekMs >= 0) return;

    if (m_playProxy) {                        // one file, one clock: nothing to stitch
        setPlayhead(std::clamp(ms / 1000.0, 0.0, track().duration()));
        return;
    }
    if (m_currentClip < 0 || m_currentClip >= track().clips().size()) return;
    const Clip& c = track().clips()[m_currentClip];
    const double src = ms / 1000.0;
    const int next = m_currentClip + 1;
    const bool hasNext = next < track().clips().size();
    const double ovNext = hasNext ? track().joinOverlap(next) : 0.0;

    // a transition starts ovNext seconds before this clip's source ends
    if (hasNext && ovNext > 0.02 && src >= c.sourceOut - ovNext * c.speed) {
        loadClip(next, track().clips()[next].sourceIn, true);
        return;
    }
    if (src >= c.sourceOut - 0.03) {
        if (hasNext) loadClip(next, track().clips()[next].sourceIn, true);
        else { m_player->pause(); setPlayhead(track().duration()); }
        return;
    }
    setPlayhead(track().clipStart(m_currentClip) + std::max(0.0, src - c.sourceIn) / c.speed);
}

// ---- editing --------------------------------------------------------------------------------------------------

void MainWindow::afterEdit() {
    m_tl->refresh();
    if (track().clips().isEmpty()) {
        m_player->stop(); m_currentClip = -1; m_playProxy = false;
        setPlayhead(0);
        updatePreviewPage();
        return;
    }
    if (m_playProxy) { m_player->pause(); m_playProxy = false; }
    seek(std::min(m_tl->playhead(), std::max(0.0, track().duration() - 0.01)));
    markFxStale();
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
    if (m_exporter.isRunning()) { statusBar()->showMessage("An export is already running."); return; }
    if (!exportDialog(this, m_settings)) return;
    const QString out = QFileDialog::getSaveFileName(this, "Export", "output.mp4", "MP4 (*.mp4)");
    if (out.isEmpty()) return;
    if (m_fx.isRunning()) m_fx.cancel();
    m_playWhenReady = false;
    if (m_playProxy) { m_player->pause(); m_playProxy = false; }
    m_player->pause();
    m_exporting = true;
    m_previewMode = false;
    m_bar->setValue(0);
    startProgressDialog("Exporting video",
        QString("Writing %1  -  %2x%3 at %4 fps.")
            .arg(QFileInfo(out).fileName()).arg(m_settings.width()).arg(m_settings.height()).arg(m_settings.fps));
    statusBar()->showMessage("Exporting...");
    m_exporter.start(track(), m_texts, out, m_settings);
}

void MainWindow::quickPreview() {
    if (track().clips().isEmpty()) { statusBar()->showMessage("Add a video to the timeline first."); return; }
    if (m_exporter.isRunning()) { statusBar()->showMessage("Something is already rendering."); return; }
    if (m_fx.isRunning()) m_fx.cancel();
    m_playWhenReady = false;
    if (m_playProxy) { m_player->pause(); m_playProxy = false; }
    m_previewPath = QDir::temp().filePath("openclip_preview.mp4");
    m_exporting = true;
    m_previewMode = true;
    m_bar->setValue(0);
    startProgressDialog("Quick preview",
        "Rendering a small version with every effect, transition and title. It opens when it is ready.");
    statusBar()->showMessage("Making a quick preview...");
    m_exporter.start(track(), m_texts, m_previewPath, Exporter::previewSettings(m_settings));
}

// ---- live preview -------------------------------------------------------------------------------------------

void MainWindow::startProgressDialog(const QString& title, const QString& message) {
    if (m_exportDlg) { m_exportDlg->close(); m_exportDlg = nullptr; }
    m_exportDlg = new ExportProgressDialog(title, this);
    m_exportDlg->setAttribute(Qt::WA_DeleteOnClose);
    ExportProgressDialog* dlg = m_exportDlg;
    connect(dlg, &ExportProgressDialog::cancelRequested, this, [this] { m_exporter.cancel(); });
    connect(dlg, &QDialog::finished, this, [this, dlg] {
        if (m_exportDlg == dlg) m_exportDlg = nullptr;
        if (m_exporter.isRunning()) m_exporter.cancel();   // closed while still rendering
    });
    dlg->setMessage(message);
    dlg->show();
}

void MainWindow::closeProgressDialog(bool ok, const QString& message) {
    if (m_exportDlg) m_exportDlg->complete(ok, message);
}

void MainWindow::markFxStale() {
    m_proxyStale = true;
    ++m_fxGen;
    if (m_playProxy) { m_player->pause(); m_playProxy = false; playRawAt(m_tl->playhead(), false); }
    if (m_fx.isRunning()) m_fx.cancel();          // an old render is worthless now
    if (!m_fxEnabled) { updatePreviewPage(); return; }
    m_fxTimer->start();
    requestStillPreview();
    updatePreviewPage();
}

void MainWindow::setFxEnabled(bool on) {
    m_fxEnabled = on;
    if (m_fxToggle && m_fxToggle->isChecked() != on) { QSignalBlocker b(m_fxToggle); m_fxToggle->setChecked(on); }
    if (m_fxAction && m_fxAction->isChecked() != on) { QSignalBlocker b(m_fxAction); m_fxAction->setChecked(on); }

    if (!on) {
        if (m_stillProc) { m_stillProc->kill(); m_stillProc->deleteLater(); m_stillProc = nullptr; }
        if (m_fx.isRunning()) m_fx.cancel();
        m_playWhenReady = false;
        m_stillPix = QPixmap();
        m_stillTime = -1;
        if (m_playProxy) { m_player->pause(); m_playProxy = false; playRawAt(m_tl->playhead(), false); }
        if (m_fxBadge) m_fxBadge->setText("FX off");
        updatePreviewPage();
        statusBar()->showMessage("FX preview off: the player shows the raw clips.");
        return;
    }
    if (m_fxBadge) m_fxBadge->setText("");
    markFxStale();
    statusBar()->showMessage("FX preview on: pause to see the real look, Play for the rendered version.");
}

void MainWindow::requestStillPreview() {
    if (!m_fxEnabled || m_exporting || track().clips().isEmpty()) return;
    if (!m_stillTimer) return;
    m_stillTimer->start();
}

void MainWindow::renderStill() {
    if (!m_fxEnabled || m_exporting || track().clips().isEmpty()) return;
    if (m_stillProc) { m_stillProc->kill(); m_stillProc->waitForFinished(500); m_stillProc->deleteLater(); m_stillProc = nullptr; }

    const double at = m_tl->playhead();
    const QString out = QDir::temp().filePath(QString("openclip_still_%1.png").arg(++m_stillGen % 2));
    const QStringList args = m_stillFx.buildStillArgs(track(), m_texts, at, m_settings, out);
    if (args.isEmpty()) return;
    QFile::remove(out);

    auto* p = new QProcess(this);
    m_stillProc = p;
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
        if (p == m_stillProc) m_stillProc = nullptr;
        p->deleteLater();
    });
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p, out, at](int code, QProcess::ExitStatus st) {
        if (p != m_stillProc) { p->deleteLater(); return; }
        m_stillProc = nullptr;
        p->deleteLater();
        if (code != 0 || st != QProcess::NormalExit) return;
        QPixmap pm(out);
        if (pm.isNull()) return;
        m_stillPix = pm;
        m_stillTime = at;
        QSize target = m_stillView->size();
        if (target.width() < 64 || target.height() < 36) target = QSize(960, 540);
        m_stillView->setPixmap(pm.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        QFile::remove(out);
        updatePreviewPage();
    });
    p->start("ffmpeg", args);
}

void MainWindow::renderFxPreview() {
    if (!m_fxEnabled || m_exporting) return;
    if (track().clips().isEmpty() || m_exporter.isRunning()) return;
    if (m_fx.isRunning()) { m_fxTimer->start(); return; }   // let the cancelled job die first
    if (track().duration() < 0.2) return;
    if (track().duration() > 900.0) {               // 15 minutes of proxy is plenty
        if (m_fxBadge) m_fxBadge->setText("FX too long");
        return;
    }
    m_fxJobGen = m_fxGen;
    int slot = 1 - m_proxySlot;
    if (m_proxyFiles[slot] == m_proxyPath) slot = 1 - slot;   // never the file the player has open
    m_proxySlot = slot;
    m_fxJobPath = m_proxyFiles[m_proxySlot];
    QFile::remove(m_fxJobPath);
    if (m_fxBadge) m_fxBadge->setText("FX 0%");
    m_fx.start(track(), m_texts, m_fxJobPath, Exporter::previewSettings(m_settings));
}

void MainWindow::startProxyPlayback() {
    if (m_proxyPath.isEmpty() || !m_proxyValid || !QFileInfo::exists(m_proxyPath)) return;
    m_playProxy = true;
    m_currentClip = -1;
    m_pendingSeekMs = qint64(std::clamp(m_tl->playhead(), 0.0, std::max(0.0, track().duration())) * 1000.0);
    m_playAfterLoad = true;
    m_player->setPlaybackRate(1.0);
    if (m_fxBadge) m_fxBadge->setText("FX ready");
    const QUrl url = QUrl::fromLocalFile(m_proxyPath);
    if (m_player->source() == url) applyPending();     // already loaded: just seek and play
    else m_player->setSource(url);
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    if (!m_stillPix.isNull() && m_stillView) {
        const QSize target = m_stillView->size();
        if (target.width() > 32 && target.height() > 32)
            m_stillView->setPixmap(m_stillPix.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
