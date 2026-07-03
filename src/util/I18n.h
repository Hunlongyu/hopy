#pragma once
#include <QString>

namespace hopy {

// Detect the UI language from the system locale: Chinese shows Chinese, every
// other locale falls back to English. Call once at startup before any UI is built.
void initLanguage();

// Localise an English source string. Returns the Chinese translation when the UI
// language is Chinese and a translation exists, otherwise the English source.
QString T(const char* english);

} // namespace hopy
