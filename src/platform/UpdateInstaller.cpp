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

QString stableInstallPath(const QString& currentExePath) {
    const QFileInfo fi(currentExePath);
#if defined(Q_OS_WIN)
    return fi.path() + QStringLiteral("/hopy.exe");      // version-less, stable across updates
#else
    return currentExePath;                               // self-update is Windows-only for now
#endif
}

#if defined(Q_OS_WIN)
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* installedOut, QString* errorOut) {
    const QString target  = stableInstallPath(currentExePath);
    const QString oldPath = sideBySideOldPath(currentExePath);
    QFile::remove(oldPath);                              // clear any stale copy

    // Windows allows renaming a running exe (but not overwriting/deleting it).
    if (!QFile::rename(currentExePath, oldPath)) {
        if (errorOut) *errorOut = QStringLiteral("could not rename the running binary");
        return InstallResult::PermissionDenied;
    }
    QFile::remove(target);                               // clear the destination (a stale hopy.exe when migrating)
    if (!QFile::copy(newExePath, target)) {
        QFile::rename(oldPath, currentExePath);          // roll back
        if (errorOut) *errorOut = QStringLiteral("could not place the new binary");
        return InstallResult::Failed;
    }
    if (installedOut) *installedOut = target;
    return InstallResult::Ok;
}
#else
InstallResult applyUpdate(const QString&, const QString&, QString*, QString* errorOut) {
    if (errorOut) *errorOut = QStringLiteral("self-update is not supported on this platform yet");
    return InstallResult::NotSupported;
}
#endif

void restartApp(const QString& exePath) {
    QProcess::startDetached(exePath, {});
}

void cleanupOldBinary(const QString& currentExePath) {
    // Remove any "hopy*_old.exe" backup next to the binary. A glob (not just this
    // name's _old) also sweeps the one-time leftover from a migration off a versioned
    // filename, e.g. hopy-v0.4.0-windows-x64_old.exe after the install became hopy.exe.
    const QDir dir(QFileInfo(currentExePath).path());
    const auto backups = dir.entryList({QStringLiteral("hopy*_old.exe")}, QDir::Files);
    for (const QString& f : backups) QFile::remove(dir.filePath(f));
}

} // namespace hopy::platform
