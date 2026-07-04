#pragma once
#include <QString>

namespace hopy {

// The running app's version, e.g. "0.1.0" (sourced from CMake project(VERSION)).
QString currentVersion();

// Compare two dotted versions. Tolerates a leading 'v', treats missing
// segments as 0, ignores any '-suffix'. Returns -1 if a<b, 0 if equal, 1 if a>b.
int compareVersions(const QString& a, const QString& b);

} // namespace hopy
