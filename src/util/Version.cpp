#include "util/Version.h"
#include "BuildVersion.h"
#include <QStringList>
#include <array>

namespace hopy {

QString currentVersion() {
    return QStringLiteral(HOPY_VERSION_STRING);
}

namespace {
// Parse "v1.2.3-beta" → {1,2,3}, missing segments default to 0.
std::array<int, 3> parts(const QString& in) {
    QString s = in.trimmed();
    if (s.startsWith('v') || s.startsWith('V')) s.remove(0, 1);
    const int dash = s.indexOf('-');
    if (dash >= 0) s = s.left(dash);
    std::array<int, 3> out{0, 0, 0};
    const QStringList segs = s.split('.');
    for (int i = 0; i < 3 && i < segs.size(); ++i) out[i] = segs[i].toInt();
    return out;
}
} // namespace

int compareVersions(const QString& a, const QString& b) {
    const auto pa = parts(a);
    const auto pb = parts(b);
    for (int i = 0; i < 3; ++i) {
        if (pa[i] < pb[i]) return -1;
        if (pa[i] > pb[i]) return 1;
    }
    return 0;
}

} // namespace hopy
