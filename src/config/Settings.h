#pragma once
#include <QString>
#include <QByteArray>

namespace hopy {

enum class ConfirmMode { CopyOnly, PasteImmediately };

struct AppSettings {
    QString theme = "dark";      // "dark" | "light"
    QString hotkey = "Ctrl+Shift+V";
    int maxHistory = 100;
    int maxStorage = 200;
    ConfirmMode confirmMode = ConfirmMode::PasteImmediately;
    bool autostart = false;
    bool hoverPreview = true;
    bool spacePreview = true;
    QString previewSide = "left"; // "left" | "right"
    int windowOpacity = 100;     // 40..100
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
