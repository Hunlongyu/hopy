#include "update/UpdateDownloader.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QRegularExpression>

namespace hopy {

QString parseChecksumFor(const QByteArray& text, const QString& fileName) {
    const QString content = QString::fromUtf8(text).trimmed();
    // Single-token file: the whole content is one hash.
    if (!content.contains('\n') && !content.contains(' ')) return content.toLower();
    const QString wanted = fileName.toLower();
    for (const QString& raw : content.split('\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        const QStringList cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (cols.size() < 2) continue;
        if (cols.last().toLower() == wanted) return cols.first().toLower();
    }
    return QString();
}

UpdateDownloader::UpdateDownloader(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam) {}

void UpdateDownloader::download(const ReleaseInfo& info) {
    if (!info.hasExe() || !info.hasChecksum()) {
        emit failed(QStringLiteral("release is missing the exe or checksum asset"));
        return;
    }
    canceled_ = false;
    // Step 1: fetch checksum file (small).
    QNetworkRequest sumReq{QUrl(info.checksumAsset.url)};
    sumReq.setRawHeader("User-Agent", "hopy-updater");
    QNetworkReply* sumReply = nam_->get(sumReq);
    active_ = sumReply;
    connect(sumReply, &QNetworkReply::finished, this, [this, sumReply, info] {
        sumReply->deleteLater();
        active_ = nullptr;
        if (canceled_) return;
        if (sumReply->error() != QNetworkReply::NoError) {
            emit failed(sumReply->errorString());
            return;
        }
        const QString expected = parseChecksumFor(sumReply->readAll(), info.exeAsset.name);
        if (expected.isEmpty()) {
            emit failed(QStringLiteral("checksum for %1 not found").arg(info.exeAsset.name));
            return;
        }
        // Step 2: download the exe with progress.
        QNetworkRequest exeReq{QUrl(info.exeAsset.url)};
        exeReq.setRawHeader("User-Agent", "hopy-updater");
        QNetworkReply* exeReply = nam_->get(exeReq);
        active_ = exeReply;
        connect(exeReply, &QNetworkReply::downloadProgress, this, &UpdateDownloader::progress);
        connect(exeReply, &QNetworkReply::finished, this, [this, exeReply, expected, info] {
            exeReply->deleteLater();
            active_ = nullptr;
            if (canceled_) return;
            if (exeReply->error() != QNetworkReply::NoError) {
                emit failed(exeReply->errorString());
                return;
            }
            const QByteArray bytes = exeReply->readAll();
            const QString got = QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
            if (got.compare(expected, Qt::CaseInsensitive) != 0) {
                qWarning("update checksum mismatch: got %s expected %s",
                         qPrintable(got), qPrintable(expected));
                emit failed(QStringLiteral("downloaded file failed checksum verification"));
                return;
            }
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const QString path = QDir(dir).filePath(QStringLiteral("hopy-update.exe"));
            QFile f(path);
            if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) {
                emit failed(QStringLiteral("could not write the downloaded update"));
                return;
            }
            f.close();
            emit ready(path);
        });
    });
}

void UpdateDownloader::cancel() {
    canceled_ = true;
    if (active_) active_->abort();   // finished() fires with OperationCanceledError, swallowed above
}

} // namespace hopy
