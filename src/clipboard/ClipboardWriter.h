#pragma once
#include "storage/Record.h"
namespace hopy {
class ClipboardWriter {
public:
    static void write(const ClipboardRecord& rec, bool plainTextOnly);
    static void writeText(const QString& text);
};
}
