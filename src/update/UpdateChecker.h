#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>
#include <QByteArray>

class QNetworkAccessManager;

namespace hopy {

// Pure: parse a GitHub "releases/latest" JSON payload. Picks the first asset
// whose name ends with ".exe" as the installer, and the first ending in
// ".sha256" or named "checksums.txt" (case-insensitive) as the checksum file.
// Returns false only when the JSON is invalid or has no tag_name.
bool parseLatestRelease(const QByteArray& json, ReleaseInfo* out);

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void check();

    // Pure: is this reply GitHub's rate-limit response? HTTP 429, or 403 with
    // X-RateLimit-Remaining: 0 (the unauthenticated 60/hour limit).
    static bool isRateLimited(int httpStatus, const QByteArray& rateLimitRemaining);

signals:
    void upToDate();
    void updateAvailable(hopy::ReleaseInfo info);
    void rateLimited(qint64 resetEpochSecs);   // GitHub API rate limit — transient; resetEpochSecs = when it clears (0 if unknown)
    void failed(QString reason);

private:
    QNetworkAccessManager* nam_;
};

} // namespace hopy
