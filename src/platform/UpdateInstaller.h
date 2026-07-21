#pragma once
#include <QString>

namespace hopy::platform {

enum class InstallResult { Ok, NotSupported, PermissionDenied, Failed };

// Pure: given ".../hopy.exe" return ".../hopy_old.exe" (extension preserved).
QString sideBySideOldPath(const QString& exePath);

// Pure: the stable, version-less path an update installs to, in the current binary's
// directory — ".../hopy.exe" on Windows (so the filename never goes stale and pinned
// shortcuts survive), the unchanged path elsewhere.
QString stableInstallPath(const QString& currentExePath);

// Swap the running binary for `newExePath` using the rename-self strategy, landing on
// stableInstallPath() (so an install off a versioned filename normalises to hopy.exe).
// Windows only; other platforms return NotSupported. On success *installedOut is the
// path the new binary now lives at (restart from there). On failure the original binary
// is restored and *errorOut is set.
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* installedOut, QString* errorOut);

// Launch `exePath` detached (call right before quitting to complete the update).
void restartApp(const QString& exePath);

// Delete a leftover "<name>_old.<ext>" next to the current binary, if present.
void cleanupOldBinary(const QString& currentExePath);

} // namespace hopy::platform
