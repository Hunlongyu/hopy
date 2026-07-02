#include "platform/Autostart.h"
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDir>

namespace hopy::platform {

#if defined(Q_OS_WIN)
namespace {
QSettings runKey() {
    return QSettings(R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                     QSettings::NativeFormat);
}
} // namespace
bool setAutostart(bool enabled) {
    QSettings k = runKey();
    if (enabled) k.setValue("hopy", QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    else k.remove("hopy");
    return true;
}
bool isAutostartEnabled() { return runKey().contains("hopy"); }

#elif defined(Q_OS_MAC)
namespace { QString plistPath() { return QDir::homePath() + "/Library/LaunchAgents/com.hopy.app.plist"; } }
bool setAutostart(bool enabled) {
    if (!enabled) { QFile::remove(plistPath()); return true; }
    QDir().mkpath(QDir::homePath() + "/Library/LaunchAgents");
    QFile f(plistPath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QString exe = QCoreApplication::applicationFilePath();
    f.write(QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n"
        "  <key>Label</key><string>com.hopy.app</string>\n"
        "  <key>ProgramArguments</key><array><string>%1</string></array>\n"
        "  <key>RunAtLoad</key><true/>\n"
        "</dict></plist>\n").arg(exe).toUtf8());
    return true;
}
bool isAutostartEnabled() { return QFile::exists(plistPath()); }

#else // Linux
namespace { QString desktopPath() { return QDir::homePath() + "/.config/autostart/hopy.desktop"; } }
bool setAutostart(bool enabled) {
    if (!enabled) { QFile::remove(desktopPath()); return true; }
    QDir().mkpath(QDir::homePath() + "/.config/autostart");
    QFile f(desktopPath());
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QString exe = QCoreApplication::applicationFilePath();
    f.write(QStringLiteral(
        "[Desktop Entry]\nType=Application\nName=hopy\nExec=%1\n"
        "X-GNOME-Autostart-enabled=true\n").arg(exe).toUtf8());
    return true;
}
bool isAutostartEnabled() { return QFile::exists(desktopPath()); }
#endif

} // namespace hopy::platform
