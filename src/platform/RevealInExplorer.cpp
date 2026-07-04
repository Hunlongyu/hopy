#include "platform/RevealInExplorer.h"
#include <QtGlobal>
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
    // "/select," must reach explorer as a single argument glued to the path.
    const bool ok = fi.isDir()
        ? QProcess::startDetached(QStringLiteral("explorer"), {native})
        : QProcess::startDetached(QStringLiteral("explorer"),
                                   {QStringLiteral("/select,") + native});
    if (!ok) qWarning("revealInExplorer: failed to launch explorer for %s", qPrintable(native));
    return ok;
#else
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
#endif
}

} // namespace hopy::platform
