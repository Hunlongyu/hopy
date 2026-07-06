#include "app/UpdateService.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDownloader.h"
#include "update/UpdateConfig.h"
#include "platform/UpdateInstaller.h"
#include "util/I18n.h"
#include "util/Paths.h"
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QUrl>

namespace hopy {

namespace {
constexpr qint64 kCheckIntervalMs = 24LL * 60 * 60 * 1000;   // silent check at most once/day
QString stampPath() { return paths::dataDir() + QStringLiteral("/last_update_check"); }
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

UpdateService::UpdateService(QWidget* dialogParent, QObject* parent)
    : QObject(parent), parent_(dialogParent) {
    nam_        = new QNetworkAccessManager(this);
    checker_    = new UpdateChecker(nam_, this);
    downloader_ = new UpdateDownloader(nam_, this);

    connect(checker_, &UpdateChecker::updateAvailable, this, &UpdateService::onUpdateAvailable);
    connect(checker_, &UpdateChecker::upToDate, this, [this] {
        if (manual_) QMessageBox::information(parent_, T("Check for updates"),
                                              T("You're on the latest version."));
        busy_ = false;
    });
    connect(checker_, &UpdateChecker::rateLimited, this, [this] {
        // Transient: GitHub's unauthenticated 60/hour limit. Not a real failure,
        // so don't offer the downloads page — just tell the user to retry later.
        if (manual_) QMessageBox::information(parent_, T("Check for updates"),
                                              T("Checked too often — please try again later."));
        busy_ = false;
    });
    connect(checker_, &UpdateChecker::failed, this, [this](const QString& why) {
        if (manual_) offerReleasesPage(why);   // silent checks fail quietly
        busy_ = false;
    });
}

void UpdateService::checkManually() {
    if (busy_) return;
    busy_ = true;
    manual_ = true;
    recordCheckNow();
    checker_->check();
}
void UpdateService::checkSilently() {
    if (busy_) return;
    if (!dueForCheck(lastCheckMs(), QDateTime::currentMSecsSinceEpoch())) return;   // throttle to once/day
    busy_ = true;
    manual_ = false;
    recordCheckNow();
    checker_->check();
}

void UpdateService::onUpdateAvailable(const ReleaseInfo& info) {
    pending_ = info;
    const auto btn = QMessageBox::question(
        parent_, T("Update available"),
        T("A new version %1 is available. Update now?").arg(info.tagName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn == QMessageBox::Yes) { startDownload(info); return; }
    busy_ = false;
}

void UpdateService::startDownload(const ReleaseInfo& info) {
    auto* dlg = new QProgressDialog(T("Downloading update…"), T("Cancel"), 0, 100, parent_);
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setAutoClose(false);
    dlg->setValue(0);

    connect(downloader_, &UpdateDownloader::progress, dlg, [dlg](qint64 rec, qint64 total) {
        if (total > 0) dlg->setMaximum(100), dlg->setValue(int(rec * 100 / total));
    });
    connect(dlg, &QProgressDialog::canceled, this, [this] {
        // Best-effort: leave the running binary untouched; nothing has been swapped yet.
        offerReleasesPage(T("Update canceled."));
        busy_ = false;
    });
    connect(downloader_, &UpdateDownloader::failed, dlg, [this, dlg](const QString& why) {
        dlg->close(); dlg->deleteLater();
        offerReleasesPage(why);
        busy_ = false;
    });
    connect(downloader_, &UpdateDownloader::ready, dlg, [this, dlg](const QString& localExe) {
        dlg->close(); dlg->deleteLater();
        const QString cur = QCoreApplication::applicationFilePath();
        QString err;
        const auto r = platform::applyUpdate(localExe, cur, &err);
        if (r != platform::InstallResult::Ok) { offerReleasesPage(err); busy_ = false; return; }
        const auto restart = QMessageBox::question(
            parent_, T("Update ready"),
            T("Update installed. Restart now to apply?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (restart == QMessageBox::Yes) {
            platform::restartApp(cur);
            QCoreApplication::quit();
        }
        busy_ = false;
    });

    downloader_->download(info);
}

void UpdateService::offerReleasesPage(const QString& reason) {
    qWarning("update degraded: %s", qPrintable(reason));
    const auto btn = QMessageBox::warning(
        parent_, T("Check for updates"),
        T("Couldn't update automatically. Open the downloads page?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl(update::releasesPageUrl()));
}

} // namespace hopy
