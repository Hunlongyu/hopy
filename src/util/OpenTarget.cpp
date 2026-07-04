#include "util/OpenTarget.h"
#include <QFileInfo>
#include <QRegularExpression>

namespace hopy {

OpenTarget detectOpenTarget(const QString& text) {
    const QString s = text.trimmed();
    if (s.isEmpty()) return {};

    const QString lower = s.toLower();
    if (lower.startsWith("http://") || lower.startsWith("https://") || lower.startsWith("ftp://"))
        return {OpenKind::Url, s};
    if (lower.startsWith("www.") && !s.contains(' '))
        return {OpenKind::Url, QStringLiteral("https://") + s};

    static const QRegularExpression email(QStringLiteral("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$"));
    if (email.match(s).hasMatch())
        return {OpenKind::Email, s};

    // A file path must look path-like (absolute or containing a separator) and
    // actually exist on disk — this avoids treating a bare word as a path.
    if (s.contains('/') || s.contains('\\') || QFileInfo(s).isAbsolute()) {
        const QFileInfo fi(s);
        if (fi.exists()) return {OpenKind::Path, fi.absoluteFilePath()};
    }
    return {};
}

} // namespace hopy
