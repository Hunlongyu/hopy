#pragma once
#include <QDialog>
#include "update/ReleaseInfo.h"

class QLabel;
class QTextBrowser;
class QStackedWidget;
class QProgressBar;
class QPushButton;

namespace hopy {

// One themed, fixed-size dialog for the whole update flow. It never resizes: the
// version + release-notes block stays put while only the bottom action row swaps
// between buttons / progress / restart / error. States with no release info
// (checking / up-to-date / check error) show a centred message in the same frame.
//
// UpdateService drives it; the dialog only renders state and emits intents.
// Close / Esc / "Later" / "Cancel" all reject() — the service reacts to rejected().
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget* parent = nullptr);

    void showChecking();
    void showUpToDate(const QString& currentVersion);
    void showAvailable(const QString& currentVersion, const ReleaseInfo& info);
    void setNotes(const QString& html);     // fill the "What's new" area (async)
    void setNotesUnavailable();             // notes fetch failed — placeholder + release page
    void showDownloading(qint64 received, qint64 total);
    void showReady();
    void showCheckError(const QString& title, const QString& detail, bool offerReleasesPage);
    void showDownloadError(const QString& detail);

signals:
    void updateNow();          // "Update now"
    void restartNow();         // "Restart now"
    void retry();              // "Retry" (service decides: re-check or re-download)
    void openReleasesPage();   // "Release page" / "Downloads page"

protected:
    void mousePressEvent(QMouseEvent* ev) override;   // drag the frameless window

private:
    QWidget* buildHeader();
    QWidget* buildFlowPage();
    QWidget* buildSimplePage();

    QStackedWidget* body_ = nullptr;      // 0 = flow (has version+notes), 1 = simple (centred)

    // flow page
    QLabel* headline_ = nullptr;
    QLabel* versionLine_ = nullptr;
    QTextBrowser* notes_ = nullptr;
    QStackedWidget* actions_ = nullptr;   // 0 available, 1 downloading, 2 ready, 3 download-error
    QProgressBar* progress_ = nullptr;
    QLabel* progressText_ = nullptr;
    QLabel* dlErrorText_ = nullptr;

    // simple page
    QProgressBar* busy_ = nullptr;        // indeterminate, for "checking"
    QLabel* simpleGlyph_ = nullptr;       // ✓ / !
    QLabel* simpleTitle_ = nullptr;
    QLabel* simpleSub_ = nullptr;
    QPushButton* simplePrimary_ = nullptr;    // Close (up-to-date) / Retry (check error)
    QPushButton* simpleSecondary_ = nullptr;  // Downloads page (check error, optional)
    bool simplePrimaryRetries_ = false;       // routes simplePrimary_ to retry() vs reject()
};

} // namespace hopy
