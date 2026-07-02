#pragma once
#include <QObject>
#include <memory>
#include "config/Settings.h"

class QLocalServer;

namespace hopy {

class ClipboardRepository;
class ClipboardMonitor;
class MainWindow;
class GlobalHotkey;
class TrayIcon;
class PasteService;
struct CapturedPayload;

// Named pipe / local socket used for the single-instance handshake.
inline constexpr const char* kSingleInstanceKey = "hopy_single_instance";

class Application : public QObject {
    Q_OBJECT
public:
    Application();
    ~Application() override;
    void start();

private:
    void onPayloadCaptured(const CapturedPayload& p);
    void refreshWindow();
    void showWindow();
    void toggleWindow();    // hotkey: show if hidden, dismiss if already up
    void dismissWindow();   // hide + return focus/caret to the editor

    AppSettings settings_;
    std::unique_ptr<ClipboardRepository> repo_;
    ClipboardMonitor* monitor_ = nullptr;
    MainWindow* window_ = nullptr;
    GlobalHotkey* hotkey_ = nullptr;
    TrayIcon* tray_ = nullptr;
    PasteService* paste_ = nullptr;
    QLocalServer* instanceServer_ = nullptr;
};

} // namespace hopy
