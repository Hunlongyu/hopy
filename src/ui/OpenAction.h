#pragma once
#include "storage/Record.h"

namespace hopy {

// Content-aware "open" for a record: URL → browser, email → mail client,
// file path / Files record → file manager. Returns true if an open was
// triggered, false when there is nothing openable. Never throws.
bool openRecord(const ClipboardRecord& rec);

} // namespace hopy
