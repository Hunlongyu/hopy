#include "app/Application.h"
#include "app/UpdateService.h"
#include "storage/Database.h"
#include "storage/ClipboardRepository.h"
#include "clipboard/ClipboardMonitor.h"
#include "clipboard/PasteService.h"
#include "ui/MainWindow.h"
#include "ui/OpenAction.h"
#include "ui/Theme.h"
#include "ui/TrayIcon.h"
#include "hotkey/GlobalHotkey.h"
#include "platform/CaretProbe.h"
#include "platform/Autostart.h"
#include "platform/ForegroundWindow.h"
#include "platform/UpdateInstaller.h"
#include "util/Paths.h"
#include "util/Hash.h"
#include "util/I18n.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QDateTime>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QKeySequence>
#include <QTimer>
#include <QDir>

namespace hopy {

Application::Application() = default;
Application::~Application() = default;

void Application::start() {
    platform::cleanupOldBinary(QCoreApplication::applicationFilePath());  // remove leftover *_old.exe from a prior update
    settings_ = Settings::load();
    setLanguage(settings_.language);   // apply the saved language before any UI is built
    applyTheme(settings_.theme);
    platform::enableUiaAccessibility();   // pose as AT client so CEF editors expose their caret

    paths::ensureDir(paths::dataDir());   // SQLite can't create the db file if the dir is missing
    Database db = Database::openAt(paths::dataDir() + "/clipboard.sqlite");
    db.migrate();
    repo_ = std::make_unique<ClipboardRepository>(db);

    window_ = new MainWindow();
    updater_ = new UpdateService(window_, this);
    connect(window_, &MainWindow::updateRequested, updater_, &UpdateService::checkManually);
    QTimer::singleShot(5000, updater_, &UpdateService::checkSilently);  // silent auto-check, non-blocking
    paste_ = new PasteService(this);
    monitor_ = new ClipboardMonitor(this);
    tray_ = new TrayIcon(this);
    connect(updater_, &UpdateService::updateBadge, this, [this](bool pending, const QString& tag) {
        tray_->setUpdateBadge(pending);
        window_->setUpdateBadge(pending, tag);
    });
    updater_->primeBadge();   // show the badge immediately if a pending update was cached
    connect(tray_, &TrayIcon::checkUpdateRequested, updater_, &UpdateService::checkManually);
    connect(tray_, &TrayIcon::pauseToggled, monitor_, &ClipboardMonitor::setPaused);
    connect(tray_, &TrayIcon::autostartToggled, this, [this](bool on) {
        if (on == settings_.autostart) return;
        platform::setAutostart(on);
        settings_.autostart = on;
        Settings::save(settings_);
        window_->setSettings(settings_);   // keep the Settings-panel toggle in sync
    });
    tray_->setAutostart(settings_.autostart);
    hotkey_ = new GlobalHotkey(this);

    connect(monitor_, &ClipboardMonitor::payloadCaptured, this, &Application::onPayloadCaptured);
    monitor_->start();

    hotkey_->setShortcut(settings_.hotkey);
    connect(hotkey_, &GlobalHotkey::activated, this, &Application::toggleWindow);
    connect(tray_, &TrayIcon::showRequested, this, &Application::showWindow);
    connect(tray_, &TrayIcon::quitRequested, qApp, &QCoreApplication::quit);

    connect(window_, &MainWindow::hideRequested, window_, &QWidget::hide);
    connect(paste_, &PasteService::hideWindowRequested, window_, &QWidget::hide);
    connect(window_, &MainWindow::confirmRequested, this, [this](qint64 id, bool plainText) {
        auto rec = repo_->getById(id);
        if (!rec) return;
        const QString echo = rec->content;
        paste_->confirm(*rec, settings_.confirmMode, plainText,
                        [this, echo] { monitor_->setSelfCopyText(echo); });
        repo_->touch(id);        // re-sort this record to the top without creating a new one
        refreshWindow();
    });
    connect(window_, &MainWindow::pinToggleRequested, this,
            [this](qint64 id) { repo_->togglePin(id); refreshWindow(); });
    connect(window_, &MainWindow::favoriteToggleRequested, this,
            [this](qint64 id) { repo_->toggleFavorite(id); refreshWindow(); });
    connect(window_, &MainWindow::deleteRequested, this,
            [this](qint64 id) { repo_->remove(id); refreshWindow(); });
    connect(window_, &MainWindow::openRequested, this, [this](qint64 id) {
        if (auto rec = repo_->getById(id)) openRecord(*rec);
    });

    window_->setSettings(settings_);
    window_->setWindowOpacity(settings_.windowOpacity / 100.0);
    connect(window_, &MainWindow::settingsChanged, this, [this](const AppSettings& in) {
        AppSettings s = in;
        const bool langChanged = (s.language != settings_.language);
        if (s.theme != settings_.theme) { applyTheme(s.theme); tray_->applyTheme(); }
        if (s.hotkey != settings_.hotkey) {
            if (!hotkey_->setShortcut(s.hotkey)) {
                // Combo is taken by another program — keep the working one and
                // roll the field back so what the user sees matches reality.
                QMessageBox::warning(window_, T("Hotkey"),
                    T("Could not register the shortcut %1 — it may already be in use by another program. Please pick another.")
                        .arg(QKeySequence(s.hotkey).toString(QKeySequence::NativeText)));
                s.hotkey = settings_.hotkey;
                window_->setSettings(s);   // reflect the revert in the panel
            }
        }
        if (s.autostart != settings_.autostart) { platform::setAutostart(s.autostart); tray_->setAutostart(s.autostart); }
        window_->setWindowOpacity(s.windowOpacity / 100.0);
        window_->setSettings(s);   // apply preview side / hover / space toggles live
        settings_ = s;
        Settings::save(s);
        refreshWindow();
        if (langChanged) {
            setLanguage(s.language);
            // UI text is baked in at build time via T(); rebuild it. Deferred so we
            // never tear the settings panel down from inside its own signal.
            QTimer::singleShot(0, this, [this] {
                window_->retranslate();
                window_->setSettings(settings_);
                tray_->retranslate();
            });
        }
    });
    connect(tray_, &TrayIcon::settingsRequested, this, &Application::showWindow);
    connect(window_, &MainWindow::clearAllRequested, this, [this] {
        repo_->clear();                                   // DELETE + VACUUM (file shrinks)
        QDir(paths::imagesDir()).removeRecursively();     // drop image sidecars
        QDir(paths::richTextDir()).removeRecursively();   // drop rich-text sidecars
        refreshWindow();
    });

    // Single instance: listen so a later launch can ask us to show instead of
    // opening a second window.
    QLocalServer::removeServer(kSingleInstanceKey);   // clear any stale endpoint
    instanceServer_ = new QLocalServer(this);
    if (instanceServer_->listen(kSingleInstanceKey)) {
        connect(instanceServer_, &QLocalServer::newConnection, this, [this] {
            if (QLocalSocket* c = instanceServer_->nextPendingConnection()) {
                c->close();
                c->deleteLater();
            }
            showWindow();
        });
    }

    refreshWindow();
}

void Application::onPayloadCaptured(const CapturedPayload& p) {
    switch (p.kind) {
        case PayloadKind::Text:
            repo_->saveText(p.text);
            break;
        case PayloadKind::RichText: {
            const auto id = static_cast<qint64>(Hash::contentHash(ContentType::RichText, p.text.toUtf8()));
            const QString htmlPath = paths::richTextDir() + "/" +
                                     QString::number(static_cast<quint64>(id)) + ".html";
            QFile f(htmlPath);
            if (f.open(QIODevice::WriteOnly)) f.write(p.html.toUtf8());
            repo_->saveRichText(p.text, htmlPath, {});
            break;
        }
        case PayloadKind::Image: {
            const QByteArray bytes(reinterpret_cast<const char*>(p.image.constBits()),
                                   static_cast<int>(p.image.sizeInBytes()));
            const quint64 h = Hash::fnv1a(bytes);
            const QString imgPath = paths::imagesDir() + "/" + QString::number(h) + ".png";
            p.image.save(imgPath, "PNG");
            repo_->saveImage(imgPath, h);
            break;
        }
        case PayloadKind::Files:
            repo_->saveFiles(p.files);
            break;
        case PayloadKind::None:
            return;
    }
    repo_->cleanup(settings_.maxStorage);
    refreshWindow();
}

void Application::refreshWindow() {
    window_->setRecords(repo_->recentRecords(settings_.maxHistory));
}

void Application::showWindow() {
    // No foreground capture needed: hopy shows without activating, so the editor
    // stays foreground with a live caret the whole time.
    refreshWindow();
    window_->showAtCursor();
}

void Application::toggleWindow() {
    // The hotkey is Alt-based by default; that Alt would otherwise open the target
    // app's menu bar on release (Chromium then steals focus to its toolbar and the
    // web input loses its caret). Defeat it before doing anything else.
    platform::suppressAltMenu();
    if (window_->isVisible()) window_->hide();   // hotkey again dismisses
    else showWindow();
}

} // namespace hopy
