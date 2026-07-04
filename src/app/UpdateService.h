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
    void checkSilently();   // startup: only surface when an update is available

private:
    void onUpdateAvailable(const ReleaseInfo& info);
    void startDownload(const ReleaseInfo& info);
    void offerReleasesPage(const QString& reason);   // degrade path

    QWidget*               parent_;
    QNetworkAccessManager* nam_;
    UpdateChecker*         checker_;
    UpdateDownloader*      downloader_;
    bool                   manual_ = false;
    bool                   busy_ = false;
    ReleaseInfo            pending_;
};

} // namespace hopy
