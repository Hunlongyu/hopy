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

signals:
    void upToDate();
    void updateAvailable(hopy::ReleaseInfo info);
    void failed(QString reason);

private:
    QNetworkAccessManager* nam_;
};

} // namespace hopy
