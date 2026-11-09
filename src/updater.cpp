#include "updater.h"
#include "version.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QUrl>
#include <QVersionNumber>

// Change this one line if you ever move the project to another GitHub account/name.
static const char* kRepo = "mum8890/openclip";

#ifdef Q_OS_WIN
static constexpr bool kCanSelfInstall = true;
#else
static constexpr bool kCanSelfInstall = false;
#endif

Updater::Updater(QWidget* parent) : QObject(parent), m_parent(parent) {}

static QString stripV(QString v) {
    v = v.trimmed();
    if (v.startsWith('v') || v.startsWith('V')) v.remove(0, 1);
    return v;
}

bool Updater::isNewer(const QString& remote, const QString& local) {
    return QVersionNumber::fromString(stripV(remote)) > QVersionNumber::fromString(stripV(local));
}

bool Updater::parseRelease(const QByteArray& json, UpdateInfo& out) {
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    out.version = stripV(o.value("tag_name").toString());
    out.notes = o.value("body").toString();
    out.url.clear();
    for (const QJsonValue& a : o.value("assets").toArray()) {
        const QString name = a.toObject().value("name").toString();
        if (name.endsWith("Setup.exe", Qt::CaseInsensitive)) {
            out.url = a.toObject().value("browser_download_url").toString();
            break;
        }
    }
    // A release without the installer attached yet (still building) is ignored.
    return !out.version.isEmpty() && !out.url.isEmpty();
}

void Updater::fetchLatest(const QString& repo, Callback cb) {
    QNetworkRequest req{QUrl(QString("https://api.github.com/repos/%1/releases/latest").arg(repo))};
    req.setRawHeader("User-Agent", "OpenClip-Updater");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(15000);
    QNetworkReply* reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        UpdateInfo info;
        if (reply->error() != QNetworkReply::NoError) { cb(false, info, reply->errorString()); return; }
        if (!parseRelease(reply->readAll(), info)) { cb(false, info, "No downloadable release found."); return; }
        cb(true, info, {});
    });
}

void Updater::check(bool userInitiated) {
    fetchLatest(kRepo, [this, userInitiated](bool ok, const UpdateInfo& info, const QString& err) {
        if (!ok) {
            if (userInitiated)
                QMessageBox::information(m_parent, "Check for updates",
                    "Could not check for updates right now.\n\n" + err);
            return;
        }
        if (isNewer(info.version, OPENCLIP_VERSION)) offer(info);
        else if (userInitiated)
            QMessageBox::information(m_parent, "Check for updates",
                QString("You're up to date. OpenClip %1 is the latest version.").arg(OPENCLIP_VERSION));
    });
}

void Updater::offer(const UpdateInfo& info) {
    QString notes = info.notes.trimmed();
    if (notes.size() > 1500) notes = notes.left(1500) + "...";
    QMessageBox box(QMessageBox::Information, "Update available",
                    QString("OpenClip %1 is available (you have %2).").arg(info.version, OPENCLIP_VERSION),
                    QMessageBox::NoButton, m_parent);
    box.setInformativeText(notes.isEmpty() ? QString() : "What's new:\n\n" + notes);
    QAbstractButton* now = box.addButton(kCanSelfInstall ? "Update now" : "Download", QMessageBox::AcceptRole);
    box.addButton("Later", QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == now) download(info);
}

void Updater::download(const UpdateInfo& info) {
    if (!kCanSelfInstall) { QDesktopServices::openUrl(QUrl(info.url)); return; }

    auto* file = new QFile(QDir::temp().filePath("OpenClip-Setup-" + info.version + ".exe"), this);
    if (!file->open(QIODevice::WriteOnly)) {
        QMessageBox::warning(m_parent, "Update", "Could not write the update file.");
        file->deleteLater();
        return;
    }
    QNetworkRequest req{QUrl(info.url)};
    req.setRawHeader("User-Agent", "OpenClip-Updater");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net.get(req);

    auto* dlg = new QProgressDialog(QString("Downloading OpenClip %1...").arg(info.version),
                                    "Cancel", 0, 100, m_parent);
    dlg->setWindowTitle("Updating OpenClip");
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setMinimumDuration(0);
    dlg->setAutoClose(false);
    dlg->setAutoReset(false);
    dlg->setValue(0);

    connect(reply, &QNetworkReply::readyRead, this, [=]() { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, this, [=](qint64 got, qint64 total) {
        if (total > 0) dlg->setValue(int(got * 100 / total));
    });
    connect(dlg, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        file->write(reply->readAll());
        file->close();
        const QNetworkReply::NetworkError err = reply->error();
        const QString path = file->fileName();
        dlg->blockSignals(true);
        dlg->close();
        dlg->deleteLater();
        file->deleteLater();
        reply->deleteLater();
        if (err != QNetworkReply::NoError) {
            if (err != QNetworkReply::OperationCanceledError)
                QMessageBox::warning(m_parent, "Update", "The download failed. Please try again later.");
            return;
        }
        // Silent install over the old version, then OpenClip restarts by itself.
        QProcess::startDetached(path, {"/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                                       "/CLOSEAPPLICATIONS", "/RESTARTAPPLICATIONS"});
        QApplication::quit();
    });
}
