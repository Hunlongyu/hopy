#pragma once
#include <QFont>
#include <QIcon>
#include <QColor>

namespace hopy::icons {

// Segoe Fluent Icons (Win11) / Segoe MDL2 Assets (Win10) private-use glyphs.
namespace glyph {
constexpr char16_t Update      = 0xE72C; // Refresh
constexpr char16_t Help        = 0xE897; // Help
constexpr char16_t Info        = 0xE946; // Info
constexpr char16_t Settings    = 0xE713; // Setting
constexpr char16_t Search      = 0xE721; // Search
constexpr char16_t Text        = 0xE8FD; // BulletedList
constexpr char16_t Image       = 0xE91B; // Photo
constexpr char16_t Files       = 0xE7C3; // Page
constexpr char16_t StarOutline = 0xE734; // FavoriteStar
constexpr char16_t StarFill    = 0xE735; // FavoriteStarFill
constexpr char16_t Pin         = 0xE718; // Pin
constexpr char16_t Delete      = 0xE711; // Cancel (x)
} // namespace glyph

QFont iconFont(int pointSize = -1);
QString ch(char16_t code);
QIcon glyphIcon(char16_t code, const QColor& color, int px = 16);

// SVG icons (Ropy's Lucide set under :/icons/<name>.svg), tinted to `color`.
QPixmap svgPixmap(const QString& name, const QColor& color, int px = 16);
QIcon svgIcon(const QString& name, const QColor& color, int px = 16);
// Checkable-button icon: `off` colour normally, `on` colour when checked.
QIcon svgToggleIcon(const QString& name, const QColor& off, const QColor& on, int px = 16);

} // namespace hopy::icons
