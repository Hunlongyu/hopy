#pragma once
#include <QString>

namespace hopy::update {

inline constexpr const char* kRepoOwner = "Hunlongyu";
inline constexpr const char* kRepoName  = "hopy";

// github.com's web endpoint that 302-redirects to the latest STABLE release's tag
// page. Unlike the REST API it is NOT subject to the 60-req/hour unauthenticated
// limit (shared per public IP — brutal behind CGNAT) and it excludes pre-releases,
// matching the old api/releases/latest behaviour.
inline QString latestReleaseUrl() {
    return QStringLiteral("https://github.com/%1/%2/releases/latest")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

// Direct asset download URL (no API): github.com/<o>/<r>/releases/download/<tag>/<name>.
inline QString assetDownloadUrl(const QString& tag, const QString& name) {
    return QStringLiteral("https://github.com/%1/%2/releases/download/%3/%4")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName), tag, name);
}

inline QString releasesPageUrl() {
    return QStringLiteral("https://github.com/%1/%2/releases")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

// The project's repository home page.
inline QString repoUrl() {
    return QStringLiteral("https://github.com/%1/%2")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

// The author's GitHub profile page.
inline QString ownerUrl() {
    return QStringLiteral("https://github.com/%1").arg(QLatin1String(kRepoOwner));
}

// github.com's per-repo Atom feed of releases. A web endpoint (not the REST API),
// so it dodges the 60/hour unauthenticated limit. Each <entry> carries the
// rendered release notes as escaped HTML in <content type="html">.
inline QString releasesAtomUrl() {
    return QStringLiteral("https://github.com/%1/%2/releases.atom")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

} // namespace hopy::update
