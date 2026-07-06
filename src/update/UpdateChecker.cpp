#include "update/UpdateChecker.h"
#include "update/UpdateConfig.h"
#include "util/Version.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QXmlStreamReader>

namespace hopy {

bool releaseFromRedirect(const QByteArray& location, ReleaseInfo* out) {
    // location: https://github.com/<owner>/<repo>/releases/tag/<TAG>
    const QString loc = QString::fromUtf8(location).trimmed();
    const int i = loc.lastIndexOf(QStringLiteral("/releases/tag/"));
    if (i < 0) return false;
    const QString tag = loc.mid(i + 14).section('/', 0, 0).trimmed();   // 14 = strlen("/releases/tag/")
    if (tag.isEmpty()) return false;
    out->tagName = tag;
    out->htmlUrl = loc;
    // Assets are named by our release CI's convention, so build the download URLs
    // directly — neither the check nor the download then touches the rate-limited API.
    const QString exe = QStringLiteral("hopy-%1-windows-x64.exe").arg(tag);
    out->exeAsset      = { exe, update::assetDownloadUrl(tag, exe), 0 };
    out->checksumAsset = { QStringLiteral("checksums.txt"),
                           update::assetDownloadUrl(tag, QStringLiteral("checksums.txt")), 0 };
    return true;
}

QString notesFromAtom(const QByteArray& atomXml, const QString& tag) {
    // Walk the feed; for each <entry> capture its <link href> and <content>, and
    // return the content of the first entry whose link points at /tag/<tag>.
    // GitHub emits <content type="html"> as escaped text, so readElementText hands
    // back the unescaped HTML directly.
    const QString needle = QStringLiteral("/tag/") + tag;
    QXmlStreamReader xml(atomXml);
    bool inEntry = false;
    QString href, content;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == u"entry") {
                inEntry = true;
                href.clear();
                content.clear();
            } else if (inEntry && name == u"link") {
                const QString h = xml.attributes().value(QLatin1String("href")).toString();
                if (!h.isEmpty()) href = h;
            } else if (inEntry && name == u"content") {
                content = xml.readElementText(QXmlStreamReader::IncludeChildElements);
            }
        } else if (xml.isEndElement() && xml.name() == u"entry") {
            if (href.endsWith(needle) || href.endsWith(needle + QLatin1Char('/')))
                return content;
            inEntry = false;
        }
    }
    return QString();
}

bool UpdateChecker::isRateLimited(int httpStatus, const QByteArray& rateLimitRemaining) {
    if (httpStatus == 429) return true;
    return httpStatus == 403 && rateLimitRemaining.trimmed() == "0";
}

UpdateChecker::UpdateChecker(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), nam_(nam) {}

void UpdateChecker::check() {
    // Ask github.com (NOT api.github.com) for the latest release: it 302-redirects
    // to the tag page. This dodges the API's 60/hour-per-IP unauthenticated limit
    // (shared behind CGNAT). Read the redirect ourselves instead of following it.
    QNetworkRequest req{QUrl(update::latestReleaseUrl())};
    req.setRawHeader("User-Agent", "hopy-updater");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // The web endpoint isn't API-rate-limited, but can still 429 on abuse.
        if (isRateLimited(status, reply->rawHeader("X-RateLimit-Remaining"))) {
            qWarning("update check: rate limited");
            emit rateLimited(reply->rawHeader("X-RateLimit-Reset").toLongLong());
            return;
        }
        ReleaseInfo info;
        if (!releaseFromRedirect(reply->rawHeader("Location"), &info)) {
            const QString why = reply->error() != QNetworkReply::NoError
                ? reply->errorString()
                : QStringLiteral("could not read the latest release (HTTP %1)").arg(status);
            qWarning("update check failed: %s", qPrintable(why));
            emit failed(why);
            return;
        }
        if (compareVersions(currentVersion(), info.tagName) < 0)
            emit updateAvailable(info);
        else
            emit upToDate();
    });
}

void UpdateChecker::fetchReleaseNotes(const QString& tag) {
    QNetworkRequest req{QUrl(update::releasesAtomUrl())};
    req.setRawHeader("User-Agent", "hopy-updater");
    QNetworkReply* reply = nam_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) { emit notesFailed(); return; }
        const QString html = notesFromAtom(reply->readAll(), tag);
        if (html.isEmpty()) emit notesFailed();
        else emit releaseNotes(tag, html);
    });
}

} // namespace hopy
