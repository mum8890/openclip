#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <functional>

class QWidget;

struct UpdateInfo {
    QString version;   // "0.4.0"
    QString notes;     // release description written on GitHub
    QString url;       // direct download link of the *-Setup.exe asset
};

// Checks GitHub Releases for a newer version and offers to install it.
class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QWidget* parent);

    // userInitiated = true: also tell the user "you're up to date" / errors.
    void check(bool userInitiated);

    using Callback = std::function<void(bool ok, const UpdateInfo& info, const QString& error)>;
    void fetchLatest(const QString& repo, Callback cb);

    static bool isNewer(const QString& remote, const QString& local);
    static bool parseRelease(const QByteArray& json, UpdateInfo& out);

private:
    void offer(const UpdateInfo& info);
    void download(const UpdateInfo& info);

    QWidget* m_parent;
    QNetworkAccessManager m_net;
};
