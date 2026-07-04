#pragma once
#include <QString>

namespace hopy {

// Unicode code-point count (CJK-friendly; not UTF-16 units).
int charCount(const QString& text);
// Number of lines: count('\n') + 1; empty string => 0.
int lineCount(const QString& text);
// Human relative time; `nowMsecs` is injected for testability. UI strings via T().
// Buckets: <60s "just now"; <60m "N min ago"; same-day "N h ago";
// yesterday "Yesterday HH:mm"; same-year "MM-dd"; else "yyyy-MM-dd".
QString relativeTime(qint64 msecs, qint64 nowMsecs);

} // namespace hopy
