#pragma once
#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QProgressBar>
#include <QComboBox>
#include <QLabel>
#include "timeline.h"
#include "exporter.h"
#include "timelinewidget.h"

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
    void setSpeedOfSelected();
    void exportVideo();
    void seek(double t, bool play = false);
    void loadClip(int index, double sourceTime, bool play);
    void applyPending();
    void onPosition(qint64 ms);
    void afterEdit();
    static double probeDuration(const QString& path);
    static bool probeHasAudio(const QString& path);

    Timeline m_timeline;
    Exporter m_exporter;
    QMediaPlayer* m_player;
    QAudioOutput* m_audioOut;
    QVideoWidget* m_video;
    TimelineWidget* m_tl;
    QProgressBar* m_bar;
    QComboBox* m_fps;
    QLabel* m_timeLabel;

    int m_currentClip = -1;
    qint64 m_pendingSeekMs = -1;
    bool m_playAfterLoad = false;
};
