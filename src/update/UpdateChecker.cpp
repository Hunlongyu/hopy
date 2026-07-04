#include "update/UpdateChecker.h"
#include "update/UpdateConfig.h"
#include "util/Version.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>

namespace hopy {

bool parseLatestRelease(const QByteArray& json, ReleaseInfo* out) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    const QJsonObject root = doc.object();
    const QString tag = root.value("tag_name").toString();
    if (tag.isEmpty()) return false;

    out->tagName = tag;
    out->htmlUrl = root.value("html_url").toString();
    out->exeAsset = {};
    out->checksumAsset = {};

    for (const QJsonValue v : root.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value("name").toString();
        const QString url  = a.value("browser_download_url").toString();
        const qint64  size = static_cast<qint64>(a.value("size").toDouble());
        const QString lower = name.toLower();
        if (out->exeAsset.url.isEmpty() && lower.endsWith(".exe"))
            out->exeAsset = {name, url, size};
        else if (out->checksumAsset.url.isEmpty() &&
                 (lower.endsWith(".sha256") || lower == "checksums.txt"))
            out->checksumAsset = {name, url, size};
    }
    return true;
}

UpdateChecker::UpdateChecker(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam) {}

void UpdateChecker::check() {
    QNetworkRequest req{QUrl(update::latestReleaseApiUrl())};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "hopy-updater");
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning("update check failed: %s", qPrintable(reply->errorString()));
            emit failed(reply->errorString());
            return;
        }
        ReleaseInfo info;
        if (!parseLatestRelease(reply->readAll(), &info)) {
            emit failed(QStringLiteral("could not parse release info"));
            return;
        }
        if (compareVersions(currentVersion(), info.tagName) < 0)
            emit updateAvailable(info);
        else
            emit upToDate();
    });
}

} // namespace hopy
