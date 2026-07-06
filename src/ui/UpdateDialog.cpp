#include "ui/UpdateDialog.h"
#include "util/I18n.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QPalette>
#include <QFrame>
#include <QMouseEvent>
#include <QWindow>
#include <QLineEdit>
#include <QAbstractButton>

namespace hopy {

namespace {
// Body pages
constexpr int kFlow = 0, kSimple = 1;
// Action pages (within the flow page)
constexpr int kAvail = 0, kDownloading = 1, kReady = 2, kDlError = 3;

QString formatMB(qint64 bytes) {
    return QString::number(bytes / 1048576.0, 'f', 1);
}
} // namespace

UpdateDialog::UpdateDialog(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);   // transparent gutter so the shadow shows
    setWindowModality(Qt::ApplicationModal);       // focus the update; keeps hopy from auto-hiding
    setFixedSize(400, 550);                        // matches MainWindow; never resizes

    // Rounded content container with a drop shadow inside a transparent margin —
    // same construction as MainWindow so the two windows look identical.
    auto* root = new QWidget(this);
    root->setObjectName("UpdateRoot");
    root->setAttribute(Qt::WA_StyledBackground, true);
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 4);
    root->setGraphicsEffect(shadow);

    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);
    rootLay->addWidget(buildHeader());
    auto* divider = new QLabel();
    divider->setObjectName("UpdateDivider");
    divider->setFixedHeight(1);
    rootLay->addWidget(divider);

    body_ = new QStackedWidget(root);
    body_->addWidget(buildFlowPage());     // kFlow
    body_->addWidget(buildSimplePage());   // kSimple
    rootLay->addWidget(body_, 1);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);   // shadow gutter
    outer->addWidget(root);
}

QWidget* UpdateDialog::buildHeader() {
    auto* h = new QWidget();
    auto* lay = new QHBoxLayout(h);
    lay->setContentsMargins(18, 13, 11, 13);
    auto* title = new QLabel(T("Check for updates"));
    title->setObjectName("UpdateHeaderTitle");
    auto* close = new QToolButton();
    close->setObjectName("UpdateClose");
    close->setText(QString::fromUtf8("\xE2\x9C\x95"));   // ✕
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QToolButton::clicked, this, &QDialog::reject);
    lay->addWidget(title);
    lay->addStretch(1);
    lay->addWidget(close);
    return h;
}

QWidget* UpdateDialog::buildFlowPage() {
    auto* p = new QWidget();
    auto* lay = new QVBoxLayout(p);
    lay->setContentsMargins(20, 18, 20, 18);
    lay->setSpacing(0);

    headline_ = new QLabel();
    headline_->setObjectName("UpdateHeadline");
    lay->addWidget(headline_);
    lay->addSpacing(6);

    versionLine_ = new QLabel();
    versionLine_->setObjectName("UpdateSub");
    versionLine_->setTextFormat(Qt::RichText);
    lay->addWidget(versionLine_);
    lay->addSpacing(16);

    auto* notesLabel = new QLabel(T("What's new"));
    notesLabel->setObjectName("UpdateSub");
    lay->addWidget(notesLabel);
    lay->addSpacing(6);

    notes_ = new QTextBrowser();
    notes_->setObjectName("UpdateNotes");
    notes_->setFrameShape(QFrame::NoFrame);
    notes_->setOpenExternalLinks(true);
    lay->addWidget(notes_, 1);
    lay->addSpacing(14);

    // Only this row changes across the flow states → the block above stays put.
    actions_ = new QStackedWidget();
    actions_->setFixedHeight(50);

    // 0 — available
    {
        auto* w = new QWidget();
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(9);
        auto* page = new QPushButton(T("Release page"));
        page->setObjectName("UpdateLink");
        page->setCursor(Qt::PointingHandCursor);
        connect(page, &QPushButton::clicked, this, &UpdateDialog::openReleasesPage);
        auto* later = new QPushButton(T("Later"));
        connect(later, &QPushButton::clicked, this, &QDialog::reject);
        auto* now = new QPushButton(T("Update now"));
        now->setObjectName("UpdatePrimary");
        now->setCursor(Qt::PointingHandCursor);
        connect(now, &QPushButton::clicked, this, &UpdateDialog::updateNow);
        l->addWidget(page);
        l->addStretch(1);
        l->addWidget(later);
        l->addWidget(now);
        actions_->addWidget(w);
    }
    // 1 — downloading
    {
        auto* w = new QWidget();
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(9);
        progress_ = new QProgressBar();
        progress_->setObjectName("UpdateProgress");
        progress_->setTextVisible(false);
        progress_->setFixedHeight(8);
        progress_->setRange(0, 100);
        v->addWidget(progress_);
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        progressText_ = new QLabel();
        progressText_->setObjectName("UpdateSub");
        auto* cancel = new QPushButton(T("Cancel"));
        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        row->addWidget(progressText_);
        row->addStretch(1);
        row->addWidget(cancel);
        v->addLayout(row);
        actions_->addWidget(w);
    }
    // 2 — ready
    {
        auto* w = new QWidget();
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(9);
        auto* ok = new QLabel(QString::fromUtf8("\xE2\x9C\x93 ") + T("Downloaded and verified"));   // ✓
        ok->setObjectName("UpdateOk");
        auto* later = new QPushButton(T("Restart later"));
        connect(later, &QPushButton::clicked, this, &QDialog::reject);
        auto* restart = new QPushButton(T("Restart now"));
        restart->setObjectName("UpdatePrimary");
        restart->setCursor(Qt::PointingHandCursor);
        connect(restart, &QPushButton::clicked, this, &UpdateDialog::restartNow);
        l->addWidget(ok);
        l->addStretch(1);
        l->addWidget(later);
        l->addWidget(restart);
        actions_->addWidget(w);
    }
    // 3 — download error
    {
        auto* w = new QWidget();
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(9);
        dlErrorText_ = new QLabel();
        dlErrorText_->setObjectName("UpdateErr");
        auto* page = new QPushButton(T("Downloads page"));
        page->setObjectName("UpdateLink");
        connect(page, &QPushButton::clicked, this, &UpdateDialog::openReleasesPage);
        auto* retry = new QPushButton(T("Retry"));
        retry->setObjectName("UpdatePrimary");
        retry->setCursor(Qt::PointingHandCursor);
        connect(retry, &QPushButton::clicked, this, &UpdateDialog::retry);
        l->addWidget(dlErrorText_);
        l->addStretch(1);
        l->addWidget(page);
        l->addWidget(retry);
        actions_->addWidget(w);
    }

    lay->addWidget(actions_);
    return p;
}

QWidget* UpdateDialog::buildSimplePage() {
    auto* p = new QWidget();
    auto* lay = new QVBoxLayout(p);
    lay->setContentsMargins(28, 20, 28, 24);
    lay->setSpacing(0);
    lay->addStretch(1);

    busy_ = new QProgressBar();
    busy_->setObjectName("UpdateProgress");
    busy_->setTextVisible(false);
    busy_->setRange(0, 0);            // indeterminate
    busy_->setFixedHeight(8);
    busy_->setFixedWidth(180);
    auto* busyRow = new QHBoxLayout();
    busyRow->addStretch(1);
    busyRow->addWidget(busy_);
    busyRow->addStretch(1);
    lay->addLayout(busyRow);

    simpleGlyph_ = new QLabel();
    simpleGlyph_->setObjectName("UpdateGlyph");
    simpleGlyph_->setAlignment(Qt::AlignCenter);
    lay->addWidget(simpleGlyph_);
    lay->addSpacing(10);

    simpleTitle_ = new QLabel();
    simpleTitle_->setObjectName("UpdateHeadline");
    simpleTitle_->setAlignment(Qt::AlignCenter);
    lay->addWidget(simpleTitle_);
    lay->addSpacing(6);

    simpleSub_ = new QLabel();
    simpleSub_->setObjectName("UpdateSub");
    simpleSub_->setAlignment(Qt::AlignCenter);
    simpleSub_->setWordWrap(true);
    lay->addWidget(simpleSub_);
    lay->addSpacing(18);

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(9);
    btnRow->addStretch(1);
    simpleSecondary_ = new QPushButton();
    simpleSecondary_->setObjectName("UpdateLink");
    connect(simpleSecondary_, &QPushButton::clicked, this, &UpdateDialog::openReleasesPage);
    simplePrimary_ = new QPushButton();
    simplePrimary_->setCursor(Qt::PointingHandCursor);
    connect(simplePrimary_, &QPushButton::clicked, this, [this] {
        if (simplePrimaryRetries_) emit retry();
        else reject();
    });
    btnRow->addWidget(simpleSecondary_);
    btnRow->addWidget(simplePrimary_);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    lay->addStretch(1);
    return p;
}

// ── State setters ─────────────────────────────────────────

void UpdateDialog::showChecking() {
    body_->setCurrentIndex(kSimple);
    busy_->show();
    simpleGlyph_->hide();
    simpleTitle_->setText(T("Checking for updates…"));
    simpleSub_->hide();
    simplePrimary_->hide();
    simpleSecondary_->hide();
}

void UpdateDialog::showUpToDate(const QString& currentVersion) {
    body_->setCurrentIndex(kSimple);
    busy_->hide();
    simpleGlyph_->setText(QString::fromUtf8("\xE2\x9C\x93"));   // ✓
    simpleGlyph_->setStyleSheet(QStringLiteral("font-size:38px; color:#3fb950;"));
    simpleGlyph_->show();
    simpleTitle_->setText(T("You're on the latest version"));
    simpleSub_->setText(QStringLiteral("v") + currentVersion);
    simpleSub_->show();
    simplePrimaryRetries_ = false;
    simplePrimary_->setText(T("Close"));
    simplePrimary_->show();
    simpleSecondary_->hide();
}

void UpdateDialog::showAvailable(const QString& currentVersion, const ReleaseInfo& info) {
    body_->setCurrentIndex(kFlow);
    headline_->setText(T("A new version is available"));
    const QString accent = palette().color(QPalette::Highlight).name();
    versionLine_->setText(T("Current %1").arg((QStringLiteral("v") + currentVersion).toHtmlEscaped())
                          + QStringLiteral("  \xE2\x86\x92  <b style=\"color:%1\">%2</b>")
                                .arg(accent, info.tagName.toHtmlEscaped()));
    notes_->setPlainText(T("Loading release notes…"));
    actions_->setCurrentIndex(kAvail);
}

void UpdateDialog::setNotes(const QString& html) {
    notes_->setHtml(html);
}

void UpdateDialog::setNotesUnavailable() {
    notes_->setPlainText(T("Release notes unavailable — see the release page."));
}

void UpdateDialog::showDownloading(qint64 received, qint64 total) {
    body_->setCurrentIndex(kFlow);
    actions_->setCurrentIndex(kDownloading);
    if (total > 0) {
        progress_->setRange(0, 100);
        progress_->setValue(int(received * 100 / total));
        if (received >= total) {
            progressText_->setText(T("Verifying…"));
        } else {
            progressText_->setText(QStringLiteral("%1 / %2 MB · %3%")
                .arg(formatMB(received), formatMB(total)).arg(int(received * 100 / total)));
        }
    } else {
        progress_->setRange(0, 0);   // unknown size → indeterminate
        progressText_->setText(QStringLiteral("%1 MB").arg(formatMB(received)));
    }
}

void UpdateDialog::showReady() {
    body_->setCurrentIndex(kFlow);
    actions_->setCurrentIndex(kReady);
}

void UpdateDialog::showCheckError(const QString& title, const QString& detail, bool offerReleasesPage) {
    body_->setCurrentIndex(kSimple);
    busy_->hide();
    simpleGlyph_->setText(QString::fromUtf8("!"));
    simpleGlyph_->setStyleSheet(QStringLiteral("font-size:36px; font-weight:bold; color:#d29922;"));
    simpleGlyph_->show();
    simpleTitle_->setText(title);
    simpleSub_->setText(detail);
    simpleSub_->show();
    simplePrimaryRetries_ = true;
    simplePrimary_->setText(T("Retry"));
    simplePrimary_->show();
    simpleSecondary_->setText(T("Downloads page"));
    simpleSecondary_->setVisible(offerReleasesPage);
}

void UpdateDialog::showDownloadError(const QString& detail) {
    body_->setCurrentIndex(kFlow);        // keep the release notes visible
    actions_->setCurrentIndex(kDlError);
    dlErrorText_->setText(QString::fromUtf8("\xE2\x9C\x95 ") + detail);   // ✕
}

void UpdateDialog::mousePressEvent(QMouseEvent* ev) {
    // Drag the frameless window when pressing a non-interactive area (header, blank
    // space). Interactive widgets keep their behaviour. Mirrors MainWindow.
    if (ev->button() == Qt::LeftButton) {
        QWidget* c = childAt(ev->position().toPoint());
        bool interactive = false;
        for (QWidget* w = c; w && w != this; w = w->parentWidget()) {
            if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QTextBrowser*>(w) ||
                qobject_cast<QLineEdit*>(w)) {
                interactive = true;
                break;
            }
        }
        if (!interactive) {
            if (QWindow* h = windowHandle()) {
                h->startSystemMove();
                return;
            }
        }
    }
    QDialog::mousePressEvent(ev);
}

} // namespace hopy
