#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>

class QWidget;
class QNetworkAccessManager;

namespace hopy {

class UpdateChecker;
class UpdateDownloader;

// Orchestrates check → confirm → download+verify → self-replace → restart.
// Any failure degrades to offering the GitHub Releases page. Never crashes.
class UpdateService : public QObject {
    Q_OBJECT
public:
    explicit UpdateService(QWidget* dialogParent, QObject* parent = nullptr);

    void checkManually();   // user-initiated: report both "up to date" and "available"
    void checkSilently();   // startup: throttled — at most one check per 24h

    // Pure: has enough time passed since the last check (epoch ms) to check again?
    // True when never checked, >= 24h elapsed, or the clock moved backwards.
    static bool dueForCheck(qint64 lastCheckMs, qint64 nowMs);

private:
    void onUpdateAvailable(const ReleaseInfo& info);
    void startDownload(const ReleaseInfo& info);
    void offerReleasesPage(const QString& reason);   // degrade path
    qint64 lastCheckMs() const;   // persisted timestamp of the last performed check
    void recordCheckNow();        // stamp "now" as the last check time

    QWidget*               parent_;
    QNetworkAccessManager* nam_;
    UpdateChecker*         checker_;
    UpdateDownloader*      downloader_;
    bool                   manual_ = false;
    bool                   busy_ = false;
    ReleaseInfo            pending_;
};

} // namespace hopy
