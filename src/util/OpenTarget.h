#pragma once
#include <QString>

namespace hopy {

enum class OpenKind { None, Url, Email, Path };
struct OpenTarget { OpenKind kind = OpenKind::None; QString value; };

// Detect an openable target in a record's plain text, priority URL → email →
// existing file path. `value` is the normalized URL / email / absolute path.
// Returns {None, ""} when nothing openable is found.
OpenTarget detectOpenTarget(const QString& text);

} // namespace hopy
