#pragma once
#include <QString>

namespace hopy::platform {

// Reveal `path` in the system file manager: select the file, or open the
// directory. Windows-native; other platforms open the containing folder.
// Returns false (doing nothing) when the path is empty or does not exist.
bool revealInExplorer(const QString& path);

} // namespace hopy::platform
