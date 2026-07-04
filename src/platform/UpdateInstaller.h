#pragma once
#include <QString>

namespace hopy::platform {

enum class InstallResult { Ok, NotSupported, PermissionDenied, Failed };

// Pure: given ".../hopy.exe" return ".../hopy_old.exe" (extension preserved).
QString sideBySideOldPath(const QString& exePath);

// Swap the running binary for `newExePath` using the rename-self strategy.
// Windows only; other platforms return NotSupported. On failure the original
// binary is restored and *errorOut is set.
InstallResult applyUpdate(const QString& newExePath, const QString& currentExePath,
                          QString* errorOut);

// Launch `exePath` detached (call right before quitting to complete the update).
void restartApp(const QString& exePath);

// Delete a leftover "<name>_old.<ext>" next to the current binary, if present.
void cleanupOldBinary(const QString& currentExePath);

} // namespace hopy::platform
