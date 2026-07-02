#include "util/ColorDetect.h"
#include <QRegularExpression>
#include <limits>

namespace hopy {

namespace {

int hexPair(const QString& s) { return s.toInt(nullptr, 16); }

QColor fromHex(const QString& h) {
    switch (h.size()) {
        case 3: return QColor(hexPair(QString(2, h[0])), hexPair(QString(2, h[1])),
                              hexPair(QString(2, h[2])));
        case 4: return QColor(hexPair(QString(2, h[0])), hexPair(QString(2, h[1])),
                              hexPair(QString(2, h[2])), hexPair(QString(2, h[3])));
        case 6: return QColor(hexPair(h.mid(0, 2)), hexPair(h.mid(2, 2)), hexPair(h.mid(4, 2)));
        case 8: return QColor(hexPair(h.mid(0, 2)), hexPair(h.mid(2, 2)),
                              hexPair(h.mid(4, 2)), hexPair(h.mid(6, 2)));   // #RRGGBBAA
        default: return QColor();
    }
}

} // namespace

QColor detectColor(const QString& text) {
    static const QRegularExpression rgbRe(
        R"(rgba?\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*(?:,\s*([0-9]*\.?[0-9]+)\s*)?\))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hexRe(
        R"(#([0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{4}|[0-9a-fA-F]{3})(?![0-9a-fA-F]))");
    // Standalone 6/8-hex token (e.g. FF5500) — bounded so it won't fire inside a
    // long hash like a git sha.
    static const QRegularExpression bareRe(
        R"((?<![0-9a-zA-Z#])([0-9a-fA-F]{8}|[0-9a-fA-F]{6})(?![0-9a-zA-Z]))");

    QColor best;
    int bestPos = std::numeric_limits<int>::max();

    if (auto m = rgbRe.match(text); m.hasMatch() && m.capturedStart() < bestPos) {
        const int r = m.captured(1).toInt(), g = m.captured(2).toInt(), b = m.captured(3).toInt();
        if (r <= 255 && g <= 255 && b <= 255) {
            QColor c(r, g, b);
            if (!m.captured(4).isEmpty()) c.setAlphaF(qBound(0.0, m.captured(4).toDouble(), 1.0));
            best = c; bestPos = m.capturedStart();
        }
    }
    if (auto m = hexRe.match(text); m.hasMatch() && m.capturedStart() < bestPos) {
        best = fromHex(m.captured(1)); bestPos = m.capturedStart();
    }
    if (auto m = bareRe.match(text); m.hasMatch() && m.capturedStart() < bestPos) {
        best = fromHex(m.captured(1)); bestPos = m.capturedStart();
    }
    return best;
}

} // namespace hopy
