#include "ui/MainWindow.h"
#include "ui/RecordListModel.h"
#include "ui/RecordDelegate.h"
#include "ui/ScreenPlacement.h"
#include "ui/IconButton.h"
#include "ui/panel/SettingsPanel.h"
#include "ui/panel/HelpPanel.h"
#include "ui/PreviewPopup.h"
#include "ui/RecordListModel.h"
#include "platform/ForegroundWindow.h"
#include "platform/InputHook.h"
#include "util/Strings.h"
#include "util/I18n.h"
#include "util/Icons.h"
#include <QApplication>
#include <QIcon>
#include <QStringList>
#include <QTimer>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListView>
#include <QLabel>
#include <QToolButton>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QWindow>
#include <QGraphicsDropShadowEffect>
#include <QAbstractButton>
#include <QComboBox>
#include <QAbstractSlider>
#include <QSpinBox>
#include <QKeySequence>
#include <QKeySequenceEdit>
#include <QAbstractItemView>
#include <QStyle>

namespace hopy {

namespace {
// IconButton re-renders its SVG on palette change, so theme switching keeps
// the icon colours correct.
QToolButton* iconButton(const QString& svgName, const QString& tip, bool checkable = false) {
    return new IconButton(svgName, tip, checkable);
}

// Preview scrolling with the mouse side buttons (M4/M5). These buttons report a
// brief press pulse, not a sustained hold, so scrolling is momentum-based: each
// press injects velocity that eases out; rapid presses accumulate into a fast,
// smooth continuous scroll.
constexpr double kSideKick     = 18.0;   // velocity (px/frame) added per press
constexpr double kSideMaxVel   = 60.0;   // velocity ceiling (~3600 px/s at 60fps)
constexpr double kSideFriction = 0.90;   // per-frame decay → gentle glide-out
constexpr double kSideStopVel  = 0.4;    // glide ends once velocity drops below this
constexpr int    kSideTickMs   = 16;     // momentum frame interval (~60fps)
} // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    // Non-activating: the window never steals focus, so the editor keeps its live
    // blinking caret (accurate placement + reliable paste). Keyboard navigation
    // arrives via the low-level hook in inputHook_ instead of OS focus.
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);  // transparent margin so the shadow shows
    setAttribute(Qt::WA_ShowWithoutActivating);  // showing must not activate us
    setFocusPolicy(Qt::NoFocus);
    resize(400, 550);

    inputHook_ = new platform::InputHook(this);
    connect(inputHook_, &platform::InputHook::dismissRequested, this, [this] {
        // A combobox/menu popup or a modal dialog we own is a separate top-level
        // window; interacting with it must NOT dismiss us (otherwise the settings
        // dropdowns close hopy the instant they open).
        if (QApplication::activePopupWidget() || QApplication::activeModalWidget())
            return;
        // Clicked away / switched windows → dismiss. Ignore the burst that can
        // fire right as we appear.
        if (isVisible() && (!showTimer_.isValid() || showTimer_.elapsed() > 200))
            hide();
    });

    // Mouse side buttons (M4/M5) momentum-scroll a visible preview.
    connect(inputHook_, &platform::InputHook::sideScroll, this, &MainWindow::onSideScroll);
    sideScrollTimer_ = new QTimer(this);
    connect(sideScrollTimer_, &QTimer::timeout, this, [this] {
        if (!preview_ || !preview_->isVisible()) { sideScrollTimer_->stop(); sideVel_ = sideAccum_ = 0.0; return; }
        sideAccum_ += sideVel_;
        const int dy = int(sideAccum_);
        sideAccum_ -= dy;
        if (dy) preview_->scrollByPixels(dy);
        sideVel_ *= kSideFriction;
        if (qAbs(sideVel_) < kSideStopVel) { sideVel_ = sideAccum_ = 0.0; sideScrollTimer_->stop(); }
    });

    // Rounded content container with a drop shadow inside a transparent margin.
    auto* root = new QWidget(this);
    root->setObjectName("Root");
    root->setAttribute(Qt::WA_StyledBackground, true);
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 4);
    root->setGraphicsEffect(shadow);

    stack_ = new QStackedWidget(root);
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->addWidget(stack_);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);   // shadow gutter
    outer->addWidget(root);

    stack_->addWidget(buildListPage());       // index 0

    buildPanels();                             // settings (index 1) + help (index 2)

    // Preview popup + hover trigger
    preview_ = new PreviewPopup(this);
    hoverTimer_ = new QTimer(this);
    hoverTimer_->setSingleShot(true);
    hoverTimer_->setInterval(180);
    connect(hoverTimer_, &QTimer::timeout, this, [this] {
        if (hoverPreview_ && hoverRow_ >= 0 && isVisible()) showPreviewRow(hoverRow_);
    });
    list_->setMouseTracking(true);
    connect(list_, &QListView::entered, this, [this](const QModelIndex& i) {
        hoverRow_ = i.row();
        if (hoverPreview_) hoverTimer_->start();
    });
    list_->viewport()->installEventFilter(this);   // catch Leave to hide the preview

    updateFilterButtons();
}

QWidget* MainWindow::buildListPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 10, 12, 12);
    layout->setSpacing(8);

    layout->addWidget(buildHeader());
    layout->addWidget(buildSearchFilterRow());

    model_ = new RecordListModel(this);
    list_ = new QListView(page);
    list_->setModel(model_);
    delegate_ = new RecordDelegate(this);
    list_->setItemDelegate(delegate_);
    // Clicks on the card's action icons -> the same actions as the keyboard shortcuts.
    connect(delegate_, &RecordDelegate::favoriteClicked, this, &MainWindow::favoriteToggleRequested);
    connect(delegate_, &RecordDelegate::pinClicked,      this, &MainWindow::pinToggleRequested);
    connect(delegate_, &RecordDelegate::deleteClicked,   this, &MainWindow::deleteRequested);
    list_->setUniformItemSizes(false);   // image rows vary in height
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setFocusPolicy(Qt::NoFocus);
    list_->viewport()->setAttribute(Qt::WA_Hover, true);   // deliver State_MouseOver to the delegate
    layout->addWidget(list_, 1);

    search_->installEventFilter(this);
    list_->installEventFilter(this);
    // Double-click a card → confirm that card, identical to pressing Enter
    // (Shift held = paste as plain text, mirroring Shift+Enter).
    connect(list_, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        const ClipboardRecord* r = model_->recordAt(index.row());
        if (!r) return;
        const bool plainText = QGuiApplication::keyboardModifiers() & Qt::ShiftModifier;
        emit confirmRequested(r->id, plainText);
    });
    connect(search_, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    return page;
}

void MainWindow::setSettings(const AppSettings& s) {
    settingsPanel_->setSettings(s);
    hoverPreview_ = s.hoverPreview;
    spacePreview_ = s.spacePreview;
    previewLeft_ = (s.previewSide != "right");
    fallbackToCenter_ = (s.windowPlacement == "center");
    if (model_) model_->setMaskSensitive(s.maskSensitive);
    if (preview_) preview_->setMaskSensitive(s.maskSensitive);

    // Content-aware open bindings.
    openKey_ = s.openKey.isEmpty()
        ? 0
        : QKeySequence(s.openKey)[0].key();   // single-key string -> key code
    openMouseButton_ = s.openMouseButton == "middle" ? Qt::MiddleButton
                     : s.openMouseButton == "none"   ? Qt::NoButton
                                                     : Qt::RightButton;

    // Build the hint label from the active bindings, e.g. "O / 右键".
    QStringList keys;
    if (!s.openKey.isEmpty())      keys << s.openKey;
    if (s.openMouseButton == "right")  keys << T("Right-click");
    else if (s.openMouseButton == "middle") keys << T("Middle-click");
    if (preview_) preview_->setOpenKeysLabel(keys.join(QStringLiteral(" / ")));
}

void MainWindow::showPreviewRow(int row) {
    const ClipboardRecord* r = model_->recordAt(row);
    if (r) {
        preview_->showPreview(*r, geometry().adjusted(16, 16, -16, -16), previewLeft_); // visible rect (drop shadow margin)
        inputHook_->setSideScrollActive(true);   // capture M4/M5 for scrolling while shown
    }
}

void MainWindow::hidePreview() {
    hoverTimer_->stop();
    sideScrollTimer_->stop();
    sideVel_ = sideAccum_ = 0.0;
    inputHook_->setSideScrollActive(false);
    if (preview_) preview_->hide();
}

// M4/M5 pressed over a visible preview. The buttons only report a press pulse (no
// hold), so each press injects velocity and the momentum ticker eases it out; rapid
// presses accumulate into a fast, smooth continuous scroll.
void MainWindow::onSideScroll(int dir, bool pressed) {
    if (!pressed) return;                    // ignore the release; momentum keeps gliding
    if (!preview_ || !preview_->isVisible()) return;
    if (dir * sideVel_ < 0) sideVel_ = 0.0;  // reversing direction → respond at once
    sideVel_ = qBound(-kSideMaxVel, sideVel_ + dir * kSideKick, kSideMaxVel);
    if (!sideScrollTimer_->isActive()) sideScrollTimer_->start(kSideTickMs);
}

void MainWindow::showList()     { stack_->setCurrentIndex(0); }
void MainWindow::showSettings() { stack_->setCurrentIndex(1); }
void MainWindow::showHelp()     { stack_->setCurrentIndex(2); }

void MainWindow::buildPanels() {
    settingsPanel_ = new SettingsPanel(this);
    helpPanel_ = new HelpPanel(this);
    stack_->addWidget(settingsPanel_);         // index 1
    stack_->addWidget(helpPanel_);             // index 2
    connect(settingsPanel_, &SettingsPanel::backRequested, this, &MainWindow::showList);
    connect(helpPanel_, &HelpPanel::backRequested, this, &MainWindow::showList);
    connect(settingsPanel_, &SettingsPanel::settingsChanged, this, &MainWindow::settingsChanged);
    connect(settingsPanel_, &SettingsPanel::checkUpdateRequested, this, &MainWindow::updateRequested);
    connect(settingsPanel_, &SettingsPanel::clearAllRequested, this, &MainWindow::clearAllRequested);
    settingsPanel_->setUpdateAvailable(pendingUpdateTag_);   // keep the label across panel rebuilds
}

void MainWindow::retranslateTips() {
    helpBtn_->setToolTip(T("Shortcuts"));
    setBtn_->setToolTip(T("Settings"));
    aaBtn_->setToolTip(T("Case sensitive"));
    wwBtn_->setToolTip(T("Whole word"));
    btnText_->setToolTip(T("Text"));
    btnImage_->setToolTip(T("Image"));
    btnFiles_->setToolTip(T("Files"));
    btnFav_->setToolTip(T("Favorites only"));
}

void MainWindow::retranslate() {
    // The list page is icon-only; translated text lives in the two panels plus a
    // few tooltips. Rebuild the panels in the current language, refresh tooltips,
    // and stay on the settings page (where the switch was made).
    stack_->removeWidget(settingsPanel_);
    stack_->removeWidget(helpPanel_);
    settingsPanel_->deleteLater();
    helpPanel_->deleteLater();
    buildPanels();
    retranslateTips();
    showSettings();
}

void MainWindow::setUpdateBadge(bool on, const QString& tag) {
    pendingUpdateTag_ = on ? tag : QString();
    if (!updateDot_) {
        updateDot_ = new QLabel(setBtn_);
        updateDot_->setObjectName("UpdateDot");
        updateDot_->setFixedSize(8, 8);
        updateDot_->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    if (on) {
        updateDot_->move(setBtn_->width() - updateDot_->width() - 2, 2);
        updateDot_->raise();
        updateDot_->show();
    } else {
        updateDot_->hide();
    }
    if (settingsPanel_) settingsPanel_->setUpdateAvailable(pendingUpdateTag_);
}

QWidget* MainWindow::buildHeader() {
    auto* w = new QWidget(this);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(QStringLiteral("Hopy"), w);
    title->setObjectName("Title");
    h->addWidget(title);
    h->addStretch(1);
    // Update-check and About moved into the Settings panel; header keeps help + settings.
    helpBtn_ = iconButton(QStringLiteral("help"),     T("Shortcuts"));
    setBtn_  = iconButton(QStringLiteral("settings"), T("Settings"));
    connect(helpBtn_, &QToolButton::clicked, this, &MainWindow::showHelp);
    connect(setBtn_,  &QToolButton::clicked, this, &MainWindow::showSettings);
    for (auto* b : {helpBtn_, setBtn_}) h->addWidget(b);
    return w;
}

static QToolButton* inSearchToggle(const QString& text, const QString& tip) {
    auto* b = new QToolButton();
    b->setText(text);
    b->setCheckable(true);
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    b->setProperty("inSearch", true);   // styled as flat grey/blue text inside the input
    b->setMaximumHeight(24);
    return b;
}

QWidget* MainWindow::buildSearchFilterRow() {
    auto* w = new QWidget(this);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    // Search box: magnifier (leading) + Aa/W toggles docked INSIDE on the right
    search_ = new QLineEdit(w);
    search_->setObjectName("SearchBox");
    search_->setPlaceholderText(QString());   // no placeholder text
    search_->setFixedHeight(36);              // fixed height so radius 18 = full pill
    searchIcon_ = new QLabel(search_);
    searchIcon_->setPixmap(icons::svgPixmap(QStringLiteral("search"), palette().color(QPalette::Mid), 16));
    searchIcon_->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* mag = searchIcon_;
    aaBtn_ = inSearchToggle(QStringLiteral("Aa"), T("Case sensitive"));
    wwBtn_ = inSearchToggle(QStringLiteral("W"), T("Whole word"));
    auto* sl = new QHBoxLayout(search_);
    sl->setContentsMargins(14, 0, 8, 0);      // magnifier sits a little in from the left edge
    sl->setSpacing(2);
    sl->addWidget(mag);
    sl->addStretch(1);
    sl->addWidget(aaBtn_);
    sl->addWidget(wwBtn_);
    search_->setTextMargins(34, 0, 62, 0);    // clear the magnifier (left) and Aa/W (right)
    h->addWidget(search_, 1);
    h->addSpacing(2);

    // Type filters grouped in a rounded pill
    auto* filterGroup = new QFrame(w);
    filterGroup->setObjectName("FilterGroup");
    auto* fg = new QHBoxLayout(filterGroup);
    fg->setContentsMargins(3, 2, 3, 2);
    fg->setSpacing(2);
    btnText_  = iconButton(QStringLiteral("filter-text"),  T("Text"), true);
    btnImage_ = iconButton(QStringLiteral("filter-image"), T("Image"), true);
    btnFiles_ = iconButton(QStringLiteral("filter-files"), T("Files"), true);
    fg->addWidget(btnText_);
    fg->addWidget(btnImage_);
    fg->addWidget(btnFiles_);
    h->addWidget(filterGroup);

    // Favorites toggle in its own matching pill
    auto* favFrame = new QFrame(w);
    favFrame->setObjectName("FilterGroup");
    auto* ff = new QHBoxLayout(favFrame);
    ff->setContentsMargins(3, 2, 3, 2);
    btnFav_ = iconButton(QStringLiteral("filter-star"), T("Favorites only"), true);
    ff->addWidget(btnFav_);
    h->addWidget(favFrame);

    connect(btnText_,  &QToolButton::clicked, this, [this] {
        setFilter(filter_ == ContentFilter::Text ? ContentFilter::All : ContentFilter::Text); });
    connect(btnImage_, &QToolButton::clicked, this, [this] {
        setFilter(filter_ == ContentFilter::Image ? ContentFilter::All : ContentFilter::Image); });
    connect(btnFiles_, &QToolButton::clicked, this, [this] {
        setFilter(filter_ == ContentFilter::Files ? ContentFilter::All : ContentFilter::Files); });
    connect(btnFav_,   &QToolButton::clicked, this, [this] {
        setFilter(filter_ == ContentFilter::Favorites ? ContentFilter::All : ContentFilter::Favorites); });
    return w;
}

void MainWindow::setRecords(const QList<ClipboardRecord>& records) {
    model_->setRecords(records);
    applyFilter();
}

void MainWindow::setFilter(ContentFilter f) {
    filter_ = f;
    updateFilterButtons();
    applyFilter();
}

void MainWindow::updateFilterButtons() {
    // Highlight the active category button; "全部" leaves them all off.
    btnText_->setChecked(filter_ == ContentFilter::Text);
    btnImage_->setChecked(filter_ == ContentFilter::Image);
    btnFiles_->setChecked(filter_ == ContentFilter::Files);
    btnFav_->setChecked(filter_ == ContentFilter::Favorites);
}

void MainWindow::applyFilter() {
    model_->setFilter(filter_, search_->text());
    if (model_->rowCount() > 0)
        list_->setCurrentIndex(model_->index(0, 0));
}

void MainWindow::showAtCursor() {
    stack_->setCurrentIndex(0);          // always open on the list page
    QList<QRect> geoms;
    for (QScreen* s : QGuiApplication::screens()) geoms << s->geometry();
    // The panel ALWAYS aims for the caret first — we never steal focus, so the
    // editor still owns a live caret we can read now (reliable every time, no
    // post-paste settling). Only when no caret can be found do we fall back to the
    // position the user chose: the mouse, or the centre of the screen.
    QPoint anchor;
    WindowPlacement mode;
    if (caretAnchorLogical(anchor)) {
        mode = WindowPlacement::Cursor;          // drop the panel just below the caret
    } else {
        anchor = QCursor::pos();
        mode = fallbackToCenter_ ? WindowPlacement::Center    // centre on the monitor under the mouse
                                 : WindowPlacement::Cursor;   // open at the mouse, Win+V style
    }
    setGeometry(placeWindow(anchor, geoms, size(), mode));
    search_->clear();
    setSearchMode(false);
    updateFilterButtons();               // keep the last-used category (do not reset)
    applyFilter();
    showTimer_.restart();
    platform::setNoActivate(winId());    // hard guarantee: this window never takes focus
    show(); raise();                     // shown WITHOUT activating (WA_ShowWithoutActivating)
    if (updateDot_ && updateDot_->isVisible())   // re-glue the badge now geometry is real
        updateDot_->move(setBtn_->width() - updateDot_->width() - 2, 2);
    inputHook_->start(this);             // route keyboard here while visible
}

void MainWindow::moveSelection(int delta) {
    const int n = model_->rowCount();
    if (n == 0) return;
    int row = list_->currentIndex().row();
    if (row < 0) row = 0;
    row = qBound(0, row + delta, n - 1);
    list_->setCurrentIndex(model_->index(row, 0));
}

qint64 MainWindow::currentId() const {
    const ClipboardRecord* r = model_->recordAt(list_->currentIndex().row());
    return r ? r->id : 0;
}

void MainWindow::confirmCurrent(bool plainText) {
    const qint64 id = currentId();
    if (id != 0) emit confirmRequested(id, plainText);
}

void MainWindow::confirmByIndex(int index) {
    const ClipboardRecord* r = model_->recordAt(index);
    if (r) emit confirmRequested(r->id, false);
}

void MainWindow::setSearchMode(bool on) {
    if (searchMode_ == on) return;
    searchMode_ = on;
    // The box never holds OS focus (the window doesn't activate), so there is no
    // blinking caret to signal search mode — cue it with an accent border (QSS
    // [searching="true"]) + a tinted magnifier.
    search_->setProperty("searching", on);
    search_->style()->unpolish(search_);
    search_->style()->polish(search_);
    search_->update();
    searchIcon_->setPixmap(icons::svgPixmap(QStringLiteral("search"),
        palette().color(on ? QPalette::Highlight : QPalette::Mid), 16));
}

bool MainWindow::handleNavKey(QKeyEvent* ev) {
    const int key = ev->key();
    const bool shift = ev->modifiers() & Qt::ShiftModifier;

    switch (key) {
        case Qt::Key_Up:      moveSelection(-1); return true;
        case Qt::Key_Down:    moveSelection(+1); return true;
        case Qt::Key_Left:    setFilter(prevFilter(filter_)); return true;
        case Qt::Key_Right:   setFilter(nextFilter(filter_)); return true;
        case Qt::Key_Tab:     setFilter(nextFilter(filter_)); return true;
        case Qt::Key_Backtab: setFilter(prevFilter(filter_)); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:   confirmCurrent(shift); return true;
        case Qt::Key_Slash:
            if (!searchMode_) { setSearchMode(true); return true; }  // "/" enters search
            return false;                                           // in search: let "/" type
        case Qt::Key_Escape:
            // First Esc leaves search (clearing the query) and returns to command
            // mode; a second Esc (already in command mode) hides the window.
            if (searchMode_) { setSearchMode(false); search_->clear(); return true; }
            emit hideRequested(); return true;
        case Qt::Key_Delete:  { const qint64 id = currentId(); if (id) emit deleteRequested(id); return true; }
        case Qt::Key_Home:    if (model_->rowCount()) { list_->setCurrentIndex(model_->index(0, 0)); showPreviewRow(0); } return true;
        case Qt::Key_End:     { const int last = model_->rowCount() - 1;
                                if (last >= 0) { list_->setCurrentIndex(model_->index(last, 0)); showPreviewRow(last); } return true; }
        default: break;
    }

    if (!searchMode_) {   // single-key shortcuts only when not typing a search
        switch (key) {
            case Qt::Key_F: { const qint64 id = currentId(); if (id) emit favoriteToggleRequested(id); return true; }
            case Qt::Key_D: { const qint64 id = currentId(); if (id) emit deleteRequested(id); return true; }
            case Qt::Key_T: { const qint64 id = currentId(); if (id) emit pinToggleRequested(id); return true; }
            case Qt::Key_1: case Qt::Key_2: case Qt::Key_3:
            case Qt::Key_4: case Qt::Key_5:
                confirmByIndex(key - Qt::Key_1); return true;
            default: break;
        }
        if (openKey_ && key == openKey_) { const qint64 id = currentId(); if (id) emit openRequested(id); return true; }
    }
    return false;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    // Keys are posted straight to MainWindow by the input hook (this window never
    // holds OS focus), so search_/list_ never see a KeyPress here — only mouse.
    // (Side buttons M4/M5 are captured by the input hook for preview scrolling, so
    // they never surface as Qt mouse events here.)
    if (ev->type() == QEvent::MouseButtonRelease && obj == list_->viewport()) {
        auto* me = static_cast<QMouseEvent*>(ev);
        if (openMouseButton_ != Qt::NoButton && me->button() == openMouseButton_) {
            const QModelIndex ix = list_->indexAt(me->pos());
            if (ix.isValid()) {
                if (const ClipboardRecord* r = model_->recordAt(ix.row())) {
                    emit openRequested(r->id);
                    return true;   // consume; QListView has no context menu to suppress
                }
            }
        }
    }
    if (ev->type() == QEvent::Leave && obj == list_->viewport()) {
        hoverRow_ = -1;
        hidePreview();
    }
    return QWidget::eventFilter(obj, ev);
}

void MainWindow::keyPressEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat() && !searchMode_) {
        if (spacePreview_) showPreviewRow(list_->currentIndex().row());
        return;
    }
    if (handleNavKey(ev)) return;
    // Unhandled key while searching → edit the query (the box has no OS focus).
    if (searchMode_) {
        if (ev->key() == Qt::Key_Backspace) {
            search_->backspace();
            if (search_->text().isEmpty()) setSearchMode(false);   // cleared → command mode
            return;
        }
        const QString t = ev->text();
        if (!t.isEmpty() && t[0].isPrint()) { search_->insert(t); return; }
    }
    QWidget::keyPressEvent(ev);
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) { hidePreview(); return; }
    QWidget::keyReleaseEvent(ev);
}

void MainWindow::hideEvent(QHideEvent* ev) {
    hidePreview();
    inputHook_->stop();   // release the keyboard so other apps type normally again
    QWidget::hideEvent(ev);
}

void MainWindow::changeEvent(QEvent* ev) {
    if ((ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange) && searchIcon_) {
        searchIcon_->setPixmap(icons::svgPixmap(QStringLiteral("search"),
                                                palette().color(QPalette::Mid), 16));
    }
    // Auto-hide is driven by the foreground-change hook now (we never activate).
    QWidget::changeEvent(ev);
}

void MainWindow::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        // Drag the frameless window when pressing a non-interactive area (title bar,
        // blank space). Interactive widgets (buttons, inputs, the list) keep their behaviour.
        QWidget* c = childAt(ev->position().toPoint());
        bool interactive = false;
        for (QWidget* w = c; w && w != this; w = w->parentWidget()) {
            if (qobject_cast<QAbstractButton*>(w) || qobject_cast<QLineEdit*>(w) ||
                qobject_cast<QAbstractItemView*>(w) || qobject_cast<QComboBox*>(w) ||
                qobject_cast<QAbstractSlider*>(w) || qobject_cast<QSpinBox*>(w) ||
                qobject_cast<QKeySequenceEdit*>(w)) {
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
    QWidget::mousePressEvent(ev);
}

} // namespace hopy
