#include "platform/RevealInExplorer.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>

namespace hopy::platform {

bool revealInExplorer(const QString& path) {
    if (path.isEmpty()) return false;
    const QFileInfo fi(path);
    if (!fi.exists()) return false;

#if defined(Q_OS_WIN)
    const QString native = QDir::toNativeSeparators(fi.absoluteFilePath());
    if (fi.isDir())
        return QProcess::startDetached(QStringLiteral("explorer"), {native});
    // "/select," must reach explorer as a single argument glued to the path.
    return QProcess::startDetached(QStringLiteral("explorer"),
                                   {QStringLiteral("/select,") + native});
#else
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
}

} // namespace hopy::platform
