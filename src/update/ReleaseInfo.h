#pragma once
#include <QString>

namespace hopy {

struct ReleaseAsset {
    QString name;
    QString url;
    qint64  size = 0;
};

struct ReleaseInfo {
    QString      tagName;
    QString      htmlUrl;
    ReleaseAsset exeAsset;
    ReleaseAsset checksumAsset;

    bool hasExe() const { return !exeAsset.url.isEmpty(); }
    bool hasChecksum() const { return !checksumAsset.url.isEmpty(); }
};

} // namespace hopy
