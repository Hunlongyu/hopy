#pragma once
#include <QString>
#include <QByteArray>

namespace hopy {

enum class ConfirmMode { CopyOnly, PasteImmediately };

struct AppSettings {
    QString theme = "dark";      // "dark" | "light"
    QString language = "auto";   // "auto" (follow system) | "zh" | "en"
    QString hotkey = "Alt+C";
    int maxHistory = 100;
    int maxStorage = 200;
    ConfirmMode confirmMode = ConfirmMode::PasteImmediately;
    bool autostart = false;
    bool suppressOnFullscreen = true;   // ignore the global hotkey while a fullscreen app is active
    bool hoverPreview = true;
    bool spacePreview = true;
    QString previewSide = "left"; // "left" | "right"
    QString windowPlacement = "cursor"; // "cursor" (follow) | "center"
    int windowOpacity = 100;     // 40..100
    QString openKey = "O";            // content-aware open: keyboard key ("" = disabled)
    QString openMouseButton = "right"; // "right" | "middle" | "none"
};

class Settings {
public:
    static AppSettings fromJson(const QByteArray& json);
    static QByteArray toJson(const AppSettings& s);
    static AppSettings load();
    static void save(const AppSettings& s);
    static QString configFilePath();
};

} // namespace hopy
