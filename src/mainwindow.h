#pragma once
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
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QVideoWidget>
#include <cmath>
#include "timeline.h"
#include "exporter.h"
#include "project.h"
#include "timelinewidget.h"
#include "updater.h"

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
    void setPlayhead(double t);
    void updateTimeUi(double t);
    void loadClip(int index, double sourceTime, bool play);
    void applyPending();
    void onPosition(qint64 ms);

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
