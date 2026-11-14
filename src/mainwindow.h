#pragma once
#include <QAction>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QMenu>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVideoWidget>
#include <cmath>
#include <functional>
#include "timeline.h"
#include "exporter.h"
#include "project.h"
#include "timelinewidget.h"
#include "updater.h"
#include "dialogs.h"

// A labelled slider (title on the left, value on the right) used in the inspector.
struct SliderRow {
    QWidget* box = nullptr;
    QSlider* slider = nullptr;
    QLabel* valueLabel = nullptr;
    double scale = 100;
    int decimals = 2;
    QString suffix;
    double value() const { return slider->value() / scale; }
    void refreshLabel() { valueLabel->setText(QString::number(value(), 'f', decimals) + suffix); }
    void set(double v) {
        { QSignalBlocker b(slider); slider->setValue(int(std::lround(v * scale))); }
        refreshLabel();
    }
};

struct Snapshot { QVector<Clip> clips; QVector<TextItem> texts; };

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
protected:
    void closeEvent(QCloseEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
private:
    Track& track() { return m_timeline.mainTrack(); }
    Clip* selClip();
    TextItem* selText();
    // take the undo snapshot FIRST, then hand out the pointer to change (never the other way round)
    Clip* editClip(const QString& key);
    TextItem* editText(const QString& key);

    // building the window
    QWidget* buildTopBar();
    QWidget* buildPlayer();
    QWidget* buildMediaPanel();
    QWidget* buildListPanel(const QString& hint, const QStringList& items, bool transitions,
                            std::function<void(const QString&)> onClick);
    QWidget* buildTemplatesPanel();
    QWidget* buildInspector();
    QWidget* buildTimelinePanel();
    QDockWidget* makeDock(const QString& title, const QString& objectName, QWidget* content);
    void buildMenus();
    void applyWorkspace(const QString& name, bool withSizes = true);

    // project
    ProjectData collect() const;
    void newProject();
    void openProject();
    bool saveProject(bool saveAs);
    bool maybeSave();
    void setDirty(bool d);
    void updateTitle();

    // undo / redo
    void checkpoint(const QString& key);
    void undo();
    void redo();
    void restore(const Snapshot& s);

    // media library + tools
    void importMedia();
    void addMediaEntry(const QString& path, double duration, bool hasAudio);
    void addSelectedMedia();
    void addMediaItem(QListWidgetItem* item);
    void applyEffect(const QString& name);
    void applyTransition(const QString& name);
    void applySpeed(double speed);
    void addTextWithStyle(const QString& style);

    // inspector
    void updateInspector();

    // playback
    void togglePlay();
    void seek(double t, bool play = false);
    void playRawAt(double t, bool play);
    void setPlayhead(double t);
    void updateTimeUi(double t);
    void updatePreviewPage();
    void loadClip(int index, double sourceTime, bool play);
    void applyPending();
    void onPosition(qint64 ms);

    // live preview: a real still frame under the playhead + a rendered proxy for playback
    void markFxStale();                 // after any edit: timers restart, old proxy is dropped
    void setFxEnabled(bool on);
    void requestStillPreview();
    void renderStill();
    void renderFxPreview();
    void startProxyPlayback();
    void startProgressDialog(const QString& title, const QString& message);
    void closeProgressDialog(bool ok, const QString& message);

    // editing / output
    void splitAtPlayhead();
    void removeSelected();
    void afterEdit();
    void newFromTemplate();
    void exportVideo();
    void quickPreview();
    static double probeDuration(const QString& path);
    static bool probeHasAudio(const QString& path);

    Timeline m_timeline;
    QVector<TextItem> m_texts;
    ExportSettings m_settings;
    Exporter m_exporter;
    Exporter m_fx;            // renders the low-res proxy used for FX playback
    Exporter m_stillFx;       // builds the still frame under the playhead
    Updater* m_updater;

    QMediaPlayer* m_player;
    QAudioOutput* m_audioOut;
    QVideoWidget* m_video;
    TimelineWidget* m_tl;
    QProgressBar* m_bar;
    QListWidget* m_media;
    QPushButton* m_playBtn;
    QSlider* m_seek;
    QLabel* m_curTime;
    QLabel* m_totTime;
    QLabel* m_projName;
    QSlider* m_zoom;
    QStackedWidget* m_inspector;
    QStackedWidget* m_previewStack;
    QLabel* m_stillView;              // page 2 of the preview: the rendered still frame
    QLabel* m_fxBadge;                // "FX 42%" / "FX ready"
    QPushButton* m_fxToggle;          // turn the live preview off on a slow PC
    QAction* m_fxAction = nullptr;    // same switch in the View menu
    QTimer* m_fxTimer;                // waits for edits to settle before re-rendering the proxy
    QTimer* m_stillTimer;             // waits for scrubbing to settle before drawing a frame
    QProcess* m_stillProc = nullptr;
    QPixmap m_stillPix;
    double m_stillTime = -1;
    int m_stillGen = 0;
    QString m_proxyFiles[2];
    QString m_proxyPath;
    QString m_fxJobPath;
    int m_proxySlot = 0;              // which of the two proxy files is written next
    bool m_proxyValid = false;
    bool m_proxyStale = true;
    bool m_fxEnabled = true;
    bool m_playProxy = false;         // the player is showing the rendered proxy
    bool m_playWhenReady = false;     // start the proxy as soon as this render finishes
    int m_fxGen = 0;
    int m_fxJobGen = 0;
    bool m_exporting = false;
    ExportProgressDialog* m_exportDlg = nullptr;

    // docks (panels you can move, tab, float and close)
    QDockWidget *m_dProject, *m_dEffects, *m_dTransitions, *m_dTitles, *m_dTemplates, *m_dProps, *m_dTimeline;

    // clip inspector
    QLabel* m_clipName;
    SliderRow m_speed, m_bright, m_contrast, m_sat, m_transDur;
    QComboBox* m_effectBox;
    QComboBox* m_transBox;
    // text inspector
    QLineEdit* m_textEdit;
    QComboBox* m_styleBox;
    QComboBox* m_posBox;
    QComboBox* m_animBox;
    QDoubleSpinBox* m_startSpin;
    QDoubleSpinBox* m_durSpin;
    SliderRow m_sizeRow;
    bool m_inspLock = false;

    // project + undo state
    QString m_projectPath;
    bool m_dirty = false;
    QList<Snapshot> m_undo, m_redo;
    QString m_lastKey;
    qint64 m_lastCheckpointMs = 0;

    int m_currentClip = -1;
    qint64 m_pendingSeekMs = -1;
    bool m_playAfterLoad = false;
    bool m_previewMode = false;
    QString m_previewPath;
};
