#pragma once
#include <QColor>
#include <QString>

namespace hopy {

// Detect the first CSS-ish colour in `text` and return it; returns an invalid
// QColor when none is found. Recognises: #RGB, #RGBA, #RRGGBB, #RRGGBBAA,
// rgb(r,g,b), rgba(r,g,b,a), and a standalone 6/8-digit hex token (e.g. FF5500).
QColor detectColor(const QString& text);

} // namespace hopy
