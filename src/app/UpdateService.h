#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>

class QWidget;
class QNetworkAccessManager;

namespace hopy {

class UpdateChecker;
class UpdateDownloader;
class UpdateDialog;

// Orchestrates check → confirm → download+verify → self-replace → restart, all
// through one themed UpdateDialog (states swap in place, no message-box chain).
// A silent startup check never opens the dialog — it only lights a badge.
class UpdateService : public QObject {
    Q_OBJECT
public:
    explicit UpdateService(QWidget* dialogParent, QObject* parent = nullptr);

    void checkManually();   // user-initiated: open the dialog and report every outcome
    void checkSilently();   // startup: throttled to once/24h; badge only, no dialog
    void primeBadge();      // startup: light the badge from the cached pending update (no network)

    // Pure: has enough time passed since the last check (epoch ms) to check again?
    // True when never checked, >= 24h elapsed, or the clock moved backwards.
    static bool dueForCheck(qint64 lastCheckMs, qint64 nowMs);

signals:
    void updateBadge(bool pending, QString tag);   // pending update (tag = version, empty when cleared)

private:
    enum class Phase { Idle, Checking, Downloading };

    void openDialog();                         // create dlg_ fresh, wire it, show "checking"
    void onRejected();                         // dialog closed: cancel any download, reset
    void onUpdateAvailable(const ReleaseInfo& info);
    void onDownloadReady(const QString& localExe);
    void startDownload();                      // download pending_
    void onRetry();
    void doRestart();
    void openReleasesPage();
    qint64 lastCheckMs() const;
    void recordCheckNow();
    void savePending(const QString& tag);   // remember a found update so the badge survives restarts
    void clearPending();                     // forget it (updated, or now up to date)
    QString loadPending() const;

    QWidget*               parent_;
    QNetworkAccessManager* nam_;
    UpdateChecker*         checker_;
    UpdateDownloader*      downloader_;
    UpdateDialog*          dlg_ = nullptr;     // alive only during a manual flow
    bool                   manual_ = false;
    bool                   busy_ = false;
    Phase                  phase_ = Phase::Idle;
    ReleaseInfo            pending_;
};

} // namespace hopy
