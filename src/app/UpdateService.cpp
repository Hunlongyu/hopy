#include "app/UpdateService.h"
#include "update/UpdateChecker.h"
#include "update/UpdateDownloader.h"
#include "update/UpdateConfig.h"
#include "platform/UpdateInstaller.h"
#include "util/I18n.h"
#include <QNetworkAccessManager>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QCoreApplication>
#include <QUrl>

namespace hopy {

UpdateService::UpdateService(QWidget* dialogParent, QObject* parent)
    : QObject(parent), parent_(dialogParent) {
    nam_        = new QNetworkAccessManager(this);
    checker_    = new UpdateChecker(nam_, this);
    downloader_ = new UpdateDownloader(nam_, this);

    connect(checker_, &UpdateChecker::updateAvailable, this, &UpdateService::onUpdateAvailable);
    connect(checker_, &UpdateChecker::upToDate, this, [this] {
        if (manual_) QMessageBox::information(parent_, T("Check for updates"),
                                              T("You're on the latest version."));
    });
    connect(checker_, &UpdateChecker::failed, this, [this](const QString& why) {
        if (manual_) offerReleasesPage(why);   // silent checks fail quietly
    });
}

void UpdateService::checkManually() { manual_ = true;  checker_->check(); }
void UpdateService::checkSilently() { manual_ = false; checker_->check(); }

void UpdateService::onUpdateAvailable(const ReleaseInfo& info) {
    pending_ = info;
    const auto btn = QMessageBox::question(
        parent_, T("Update available"),
        T("A new version %1 is available. Update now?").arg(info.tagName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (btn == QMessageBox::Yes) startDownload(info);
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
    });
    connect(downloader_, &UpdateDownloader::failed, dlg, [this, dlg](const QString& why) {
        dlg->close(); dlg->deleteLater();
        offerReleasesPage(why);
    });
    connect(downloader_, &UpdateDownloader::ready, dlg, [this, dlg](const QString& localExe) {
        dlg->close(); dlg->deleteLater();
        const QString cur = QCoreApplication::applicationFilePath();
        QString err;
        const auto r = platform::applyUpdate(localExe, cur, &err);
        if (r != platform::InstallResult::Ok) { offerReleasesPage(err); return; }
        const auto restart = QMessageBox::question(
            parent_, T("Update ready"),
            T("Update installed. Restart now to apply?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (restart == QMessageBox::Yes) {
            platform::restartApp(cur);
            QCoreApplication::quit();
        }
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
