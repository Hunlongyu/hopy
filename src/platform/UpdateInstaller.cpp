#include "platform/UpdateInstaller.h"
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QProcess>

namespace hopy::platform {

QString sideBySideOldPath(const QString& exePath) {
    const QFileInfo fi(exePath);
    const QString base = fi.completeBaseName();          // "hopy"
    const QString ext  = fi.suffix();                    // "exe" or ""
    const QString name = ext.isEmpty() ? base + QStringLiteral("_old")
                                       : base + QStringLiteral("_old.") + ext;
    return fi.path() + QLatin1Char('/') + name;
}

#if defined(Q_OS_WIN)
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* errorOut) {
    const QString oldPath = sideBySideOldPath(currentExePath);
    QFile::remove(oldPath);                              // clear any stale copy

    // Windows allows renaming a running exe (but not overwriting/deleting it).
    if (!QFile::rename(currentExePath, oldPath)) {
        if (errorOut) *errorOut = QStringLiteral("could not rename the running binary");
        return InstallResult::PermissionDenied;
    }
    if (!QFile::copy(newExePath, currentExePath)) {
        QFile::rename(oldPath, currentExePath);          // roll back
        if (errorOut) *errorOut = QStringLiteral("could not place the new binary");
        return InstallResult::Failed;
    }
    return InstallResult::Ok;
}
#else
InstallResult applyUpdate(const QString&, const QString&, QString* errorOut) {
    if (errorOut) *errorOut = QStringLiteral("self-update is not supported on this platform yet");
    return InstallResult::NotSupported;
}
#endif

void restartApp(const QString& exePath) {
    QProcess::startDetached(exePath, {});
}

void cleanupOldBinary(const QString& currentExePath) {
    const QString oldPath = sideBySideOldPath(currentExePath);
    if (QFile::exists(oldPath)) QFile::remove(oldPath);
}

} // namespace hopy::platform
