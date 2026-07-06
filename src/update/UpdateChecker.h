#pragma once
#include "update/ReleaseInfo.h"
#include <QObject>
#include <QByteArray>

class QNetworkAccessManager;

namespace hopy {

// Pure: turn the "Location" header of github.com/…/releases/latest (which 302-
// redirects to …/releases/tag/<TAG>) into a ReleaseInfo — the tag plus the
// convention-named Windows exe + checksums.txt direct download URLs. False if the
// header carries no /releases/tag/<TAG> segment.
bool releaseFromRedirect(const QByteArray& location, ReleaseInfo* out);

// Pure: extract the release-notes HTML for `tag` from a GitHub releases.atom feed.
// Returns the <content> of the <entry> whose <link href> ends with "/tag/<tag>"
// (exact tag, not a suffix match), with its escaped HTML unescaped. Empty string
// if the feed carries no such entry.
QString notesFromAtom(const QByteArray& atomXml, const QString& tag);

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void check();

    // Fetch the release notes for `tag` from the Atom feed (see notesFromAtom).
    // Emits releaseNotes(tag, html) on success, notesFailed() otherwise. Best-effort:
    // the update flow works without notes, so failure is not fatal.
    void fetchReleaseNotes(const QString& tag);

    // Pure: is this reply GitHub's rate-limit response? HTTP 429, or 403 with
    // X-RateLimit-Remaining: 0 (the unauthenticated 60/hour limit).
    static bool isRateLimited(int httpStatus, const QByteArray& rateLimitRemaining);

signals:
    void upToDate();
    void updateAvailable(hopy::ReleaseInfo info);
    void rateLimited(qint64 resetEpochSecs);   // GitHub API rate limit — transient; resetEpochSecs = when it clears (0 if unknown)
    void failed(QString reason);
    void releaseNotes(QString tag, QString html);   // fetchReleaseNotes succeeded
    void notesFailed();                             // fetchReleaseNotes couldn't get notes (non-fatal)

private:
    QNetworkAccessManager* nam_;
};

} // namespace hopy
