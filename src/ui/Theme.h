#pragma once
#include <QString>

namespace hopy {

// Apply "dark" or "light" theme: sets the application palette (so the custom
// delegate auto-adapts) and loads the matching QSS from resources.
void applyTheme(const QString& name);

} // namespace hopy
