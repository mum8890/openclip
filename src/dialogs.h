#pragma once
#include <QDialog>
#include <QElapsedTimer>
#include "timeline.h"
#include "templates.h"

class QLabel;
class QProgressBar;
class QPushButton;

bool editClipDialog(QWidget* parent, Clip& clip);
bool addTextDialog(QWidget* parent, TextItem& item);
bool exportDialog(QWidget* parent, ExportSettings& s, const QString& title = "Export");
bool templateDialog(QWidget* parent, VideoTemplate& chosen, QVector<SlotPick>& picks);

// Big percentage, progress bar, time remaining and a Cancel button while FFmpeg works.
class ExportProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportProgressDialog(const QString& title, QWidget* parent = nullptr);

    void setProgress(double fraction);          // 0..1
    void setMessage(const QString& message);
    void complete(bool ok, const QString& message);
    bool wasCancelled() const { return m_cancelled; }

signals:
    void cancelRequested();

private:
    QLabel* m_percent;
    QLabel* m_status;
    QLabel* m_timing;
    QProgressBar* m_bar;
    QPushButton* m_cancel;
    QElapsedTimer m_clock;
    bool m_cancelled = false;
    bool m_done = false;
};
