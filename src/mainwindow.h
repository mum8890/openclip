#pragma once
#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QProgressBar>
#include <QLabel>
#include "timeline.h"
#include "exporter.h"
#include "timelinewidget.h"
#include "updater.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
private:
    Track& track() { return m_timeline.mainTrack(); }
    void addClips();
    void togglePlay();
    void splitAtPlayhead();
    void removeSelected();
    void editSelected();
    void addText();
    void newFromTemplate();
    void exportVideo();
    void quickPreview();
    void seek(double t, bool play = false);
    void loadClip(int index, double sourceTime, bool play);
    void applyPending();
    void onPosition(qint64 ms);
    void afterEdit();
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
    QLabel* m_timeLabel;

    int m_currentClip = -1;
    qint64 m_pendingSeekMs = -1;
    bool m_playAfterLoad = false;
    bool m_previewMode = false;
    QString m_previewPath;
};
