#include "app/UpdateService.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDownloader.h"
#include "update/UpdateConfig.h"
#include "ui/UpdateDialog.h"
#include "platform/UpdateInstaller.h"
#include "util/I18n.h"
#include "util/Paths.h"
#include "util/Version.h"
#include <QNetworkAccessManager>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QUrl>

namespace hopy {

namespace {
constexpr qint64 kCheckIntervalMs = 24LL * 60 * 60 * 1000;   // silent check at most once/day
QString stampPath()   { return paths::dataDir() + QStringLiteral("/last_update_check"); }
QString pendingPath() { return paths::dataDir() + QStringLiteral("/pending_update"); }
}

bool UpdateService::dueForCheck(qint64 lastCheckMs, qint64 nowMs) {
    if (lastCheckMs <= 0) return true;                   // never checked
    const qint64 elapsed = nowMs - lastCheckMs;
    return elapsed < 0 || elapsed >= kCheckIntervalMs;   // clock skew, or a full interval passed
}

qint64 UpdateService::lastCheckMs() const {
    QFile f(stampPath());
    if (!f.open(QIODevice::ReadOnly)) return 0;
    return f.readAll().trimmed().toLongLong();
}

void UpdateService::recordCheckNow() {
    paths::ensureDir(paths::dataDir());
    QFile f(stampPath());
    if (f.open(QIODevice::WriteOnly))
        f.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
}

void UpdateService::savePending(const QString& tag) {
    paths::ensureDir(paths::dataDir());
    QFile f(pendingPath());
    if (f.open(QIODevice::WriteOnly)) f.write(tag.toUtf8());
}

void UpdateService::clearPending() {
    QFile::remove(pendingPath());
}

QString UpdateService::loadPending() const {
    QFile f(pendingPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}

void UpdateService::primeBadge() {
    // Show the badge straight away if a previously-found update is still newer than
    // us — the badge must survive restarts, independent of the throttled network check.
    const QString tag = loadPending();
    if (tag.isEmpty()) return;
    if (compareVersions(currentVersion(), tag) < 0) emit updateBadge(true, tag);
    else clearPending();   // cached update is stale (already on it or newer)
}

UpdateService::UpdateService(QWidget* dialogParent, QObject* parent)
    : QObject(parent), parent_(dialogParent) {
    nam_        = new QNetworkAccessManager(this);
    checker_    = new UpdateChecker(nam_, this);
    downloader_ = new UpdateDownloader(nam_, this);

    // ── Checker → dialog states ──────────────────────────
    connect(checker_, &UpdateChecker::updateAvailable, this, &UpdateService::onUpdateAvailable);
    connect(checker_, &UpdateChecker::upToDate, this, [this] {
        clearPending();                // no update → drop any cached one and clear the badge
        emit updateBadge(false, QString());
        if (manual_) { if (dlg_) dlg_->showUpToDate(currentVersion()); }
        else busy_ = false;
    });
    connect(checker_, &UpdateChecker::rateLimited, this, [this](qint64 resetEpochSecs) {
        // Transient: GitHub's unauthenticated limit. Tell the user when it clears;
        // don't offer the downloads page (nothing is actually wrong).
        if (manual_) {
            QString msg = T("Checked too often — please try again later.");
            if (resetEpochSecs > 0)
                msg = T("Checked too often — please try again after %1.")
                          .arg(QDateTime::fromSecsSinceEpoch(resetEpochSecs).toString(QStringLiteral("HH:mm")));
            if (dlg_) dlg_->showCheckError(T("Checked too often"), msg, false);
        } else busy_ = false;
    });
    connect(checker_, &UpdateChecker::failed, this, [this](const QString& why) {
        if (manual_) { if (dlg_) dlg_->showCheckError(T("Update check failed"), why, true); }
        else busy_ = false;   // silent checks fail quietly
    });
    connect(checker_, &UpdateChecker::releaseNotes, this, [this](const QString&, const QString& html) {
        if (dlg_) dlg_->setNotes(html);
    });
    connect(checker_, &UpdateChecker::notesFailed, this, [this] {
        if (dlg_) dlg_->setNotesUnavailable();
    });

    // ── Downloader → dialog states ───────────────────────
    connect(downloader_, &UpdateDownloader::progress, this, [this](qint64 rec, qint64 total) {
        if (dlg_) dlg_->showDownloading(rec, total);
    });
    connect(downloader_, &UpdateDownloader::ready, this, [this](const QString& localExe) {
        onDownloadReady(localExe);
    });
    connect(downloader_, &UpdateDownloader::failed, this, [this](const QString&) {
        // Keep phase_ == Downloading so "Retry" re-downloads; cancel() on close is a no-op.
        if (dlg_) dlg_->showDownloadError(T("Download failed"));
    });
}

void UpdateService::openDialog() {
    dlg_ = new UpdateDialog(parent_);
    dlg_->setAttribute(Qt::WA_DeleteOnClose);
    connect(dlg_, &QDialog::rejected,          this, &UpdateService::onRejected);
    connect(dlg_, &QObject::destroyed,         this, [this] { dlg_ = nullptr; });
    connect(dlg_, &UpdateDialog::updateNow,        this, &UpdateService::startDownload);
    connect(dlg_, &UpdateDialog::restartNow,       this, &UpdateService::doRestart);
    connect(dlg_, &UpdateDialog::retry,            this, &UpdateService::onRetry);
    connect(dlg_, &UpdateDialog::openReleasesPage, this, &UpdateService::openReleasesPage);
    dlg_->showChecking();
    dlg_->show();
    dlg_->raise();
    dlg_->activateWindow();
}

void UpdateService::checkManually() {
    if (busy_) {                       // a flow is already open — just resurface it
        if (dlg_) { dlg_->raise(); dlg_->activateWindow(); }
        return;
    }
    busy_ = true;
    manual_ = true;
    phase_ = Phase::Checking;
    recordCheckNow();
    openDialog();
    checker_->check();                 // the result drives the badge (available/up-to-date)
}

void UpdateService::checkSilently() {
    if (busy_) return;
    if (!dueForCheck(lastCheckMs(), QDateTime::currentMSecsSinceEpoch())) return;   // throttle to once/day
    busy_ = true;
    manual_ = false;
    phase_ = Phase::Checking;
    recordCheckNow();
    checker_->check();                 // no dialog; result only flips the badge
}

void UpdateService::onUpdateAvailable(const ReleaseInfo& info) {
    pending_ = info;
    savePending(info.tagName);         // remember it so the badge survives restarts
    emit updateBadge(true, info.tagName);
    if (manual_) {
        if (dlg_) {
            dlg_->showAvailable(currentVersion(), info);
            checker_->fetchReleaseNotes(info.tagName);   // fill "What's new" when it arrives
        }
    } else {
        busy_ = false;                 // silent: badge only, no dialog
    }
}

void UpdateService::startDownload() {
    if (!pending_.hasExe()) return;
    phase_ = Phase::Downloading;
    if (dlg_) dlg_->showDownloading(0, 0);
    downloader_->download(pending_);
}

void UpdateService::onDownloadReady(const QString& localExe) {
    const QString cur = QCoreApplication::applicationFilePath();
    QString err, installed;
    const auto r = platform::applyUpdate(localExe, cur, &installed, &err);
    if (r != platform::InstallResult::Ok) {
        qWarning("update install failed: %s", qPrintable(err));
        if (dlg_) dlg_->showDownloadError(T("Couldn't install the update"));
        return;                        // leave phase_ so "Retry" re-downloads
    }
    installedPath_ = installed;        // the new binary may have landed on a new name (hopy.exe) — restart from there
    phase_ = Phase::Idle;              // installed; closing now must not cancel anything
    clearPending();                    // done with this update → clear the badge
    emit updateBadge(false, QString());
    if (dlg_) dlg_->showReady();
}

void UpdateService::onRetry() {
    if (phase_ == Phase::Downloading) {
        startDownload();               // re-download after a download/install failure
    } else {
        phase_ = Phase::Checking;      // re-check after a check failure
        if (dlg_) dlg_->showChecking();
        recordCheckNow();
        checker_->check();
    }
}

void UpdateService::onRejected() {
    if (phase_ == Phase::Downloading) downloader_->cancel();   // abort an in-flight download
    phase_ = Phase::Idle;
    busy_ = false;
}

void UpdateService::doRestart() {
    // Restart the binary we just installed: after a migration the running exe's own
    // path (applicationFilePath) is the now-renamed old name, so prefer installedPath_.
    const QString exe = installedPath_.isEmpty() ? QCoreApplication::applicationFilePath()
                                                 : installedPath_;
    platform::restartApp(exe);
    QCoreApplication::quit();
}

void UpdateService::openReleasesPage() {
    QDesktopServices::openUrl(QUrl(update::releasesPageUrl()));
}

} // namespace hopy
