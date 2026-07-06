#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>
#include <QByteArray>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace hopy {

// Pure: extract the SHA-256 for `fileName` from a checksums payload.
// Supports "<hex>  <name>" lines (sha256sum style; name matched case-insensitively)
// and a single-token file (the whole content is one hash). Returns lowercase hex,
// or an empty string if not found.
QString parseChecksumFor(const QByteArray& text, const QString& fileName);

class UpdateDownloader : public QObject {
    Q_OBJECT
public:
    explicit UpdateDownloader(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void download(const ReleaseInfo& info);
    void cancel();   // abort an in-flight download; emits nothing (the caller drove the cancel)

signals:
    void progress(qint64 received, qint64 total);
    void ready(QString localExePath);
    void failed(QString reason);

private:
    QNetworkAccessManager* nam_;
    QNetworkReply* active_ = nullptr;   // current in-flight reply, so cancel() can abort it
    bool canceled_ = false;             // set by cancel() to swallow the resulting finished()
};

} // namespace hopy
