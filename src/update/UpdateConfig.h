#pragma once
#include <QString>

namespace hopy::update {

inline constexpr const char* kRepoOwner = "Hunlongyu";
inline constexpr const char* kRepoName  = "hopy";

inline QString latestReleaseApiUrl() {
    return QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

inline QString releasesPageUrl() {
    return QStringLiteral("https://github.com/%1/%2/releases")
        .arg(QLatin1String(kRepoOwner), QLatin1String(kRepoName));
}

} // namespace hopy::update
