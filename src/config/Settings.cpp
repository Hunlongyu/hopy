#include "config/Settings.h"
#include "util/Paths.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <algorithm>

namespace hopy {

AppSettings Settings::fromJson(const QByteArray& json) {
    AppSettings s;
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    if (o.contains("theme"))       s.theme = o["theme"].toString(s.theme);
    if (o.contains("language"))    s.language = o["language"].toString(s.language);
    if (o.contains("hotkey"))      s.hotkey = o["hotkey"].toString(s.hotkey);
    if (o.contains("maxHistory"))  s.maxHistory = o["maxHistory"].toInt(s.maxHistory);
    if (o.contains("maxStorage"))  s.maxStorage = o["maxStorage"].toInt(s.maxStorage);
    if (o.contains("confirmMode"))
        s.confirmMode = o["confirmMode"].toString() == "copy" ? ConfirmMode::CopyOnly
                                                              : ConfirmMode::PasteImmediately;
    if (o.contains("autostart"))    s.autostart = o["autostart"].toBool(s.autostart);
    if (o.contains("suppressOnFullscreen")) s.suppressOnFullscreen = o["suppressOnFullscreen"].toBool(s.suppressOnFullscreen);
    if (o.contains("hoverPreview")) s.hoverPreview = o["hoverPreview"].toBool(s.hoverPreview);
    if (o.contains("spacePreview")) s.spacePreview = o["spacePreview"].toBool(s.spacePreview);
    if (o.contains("previewSide"))  s.previewSide = o["previewSide"].toString(s.previewSide);
    if (o.contains("windowPlacement")) s.windowPlacement = o["windowPlacement"].toString(s.windowPlacement);
    if (o.contains("windowOpacity"))s.windowOpacity = o["windowOpacity"].toInt(s.windowOpacity);
    if (o.contains("openKey"))         s.openKey = o["openKey"].toString(s.openKey);
    if (o.contains("openMouseButton")) s.openMouseButton = o["openMouseButton"].toString(s.openMouseButton);

    if (s.theme != "dark" && s.theme != "light") s.theme = "dark";
    if (s.language != "zh" && s.language != "en" && s.language != "auto") s.language = "auto";
    if (s.previewSide != "left" && s.previewSide != "right") s.previewSide = "left";
    if (s.windowPlacement != "cursor" && s.windowPlacement != "center") s.windowPlacement = "cursor";
    s.maxHistory = std::clamp(s.maxHistory, 10, 1000);
    s.maxStorage = std::clamp(s.maxStorage, 10, 1000);
    if (s.maxStorage < s.maxHistory) s.maxStorage = s.maxHistory;
    s.windowOpacity = std::clamp(s.windowOpacity, 60, 100);   // floor keeps the window recoverable
    if (s.openMouseButton != "right" && s.openMouseButton != "middle" && s.openMouseButton != "none")
        s.openMouseButton = "right";
    return s;
}

QByteArray Settings::toJson(const AppSettings& s) {
    QJsonObject o;
    o["theme"] = s.theme;
    o["language"] = s.language;
    o["hotkey"] = s.hotkey;
    o["maxHistory"] = s.maxHistory;
    o["maxStorage"] = s.maxStorage;
    o["confirmMode"] = s.confirmMode == ConfirmMode::CopyOnly ? "copy" : "paste";
    o["autostart"] = s.autostart;
    o["suppressOnFullscreen"] = s.suppressOnFullscreen;
    o["hoverPreview"] = s.hoverPreview;
    o["spacePreview"] = s.spacePreview;
    o["previewSide"] = s.previewSide;
    o["windowPlacement"] = s.windowPlacement;
    o["windowOpacity"] = s.windowOpacity;
    o["openKey"] = s.openKey;
    o["openMouseButton"] = s.openMouseButton;
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

QString Settings::configFilePath() { return paths::dataDir() + "/config.json"; }

AppSettings Settings::load() {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) return fromJson("{}");
    return fromJson(f.readAll());
}

void Settings::save(const AppSettings& s) {
    paths::ensureDir(paths::dataDir());
    QFile f(configFilePath());
    if (f.open(QIODevice::WriteOnly)) f.write(toJson(s));
    else qWarning("hopy: failed to save settings");
}

} // namespace hopy
