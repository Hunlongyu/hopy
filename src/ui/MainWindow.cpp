#include "ui/MainWindow.h"
#include "ui/RecordListModel.h"
#include "ui/RecordDelegate.h"
#include "ui/ScreenPlacement.h"
#include "ui/IconButton.h"
#include "ui/panel/SettingsPanel.h"
#include "ui/panel/HelpPanel.h"
#include "ui/PreviewPopup.h"
#include "ui/RecordListModel.h"
#include "util/Strings.h"
#include "util/Icons.h"
#include <QApplication>
#include <QIcon>
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
#include <QKeySequenceEdit>
#include <QAbstractItemView>

namespace hopy {

namespace {
// IconButton re-renders its SVG on palette change, so theme switching keeps
// the icon colours correct.
QToolButton* iconButton(const QString& svgName, const QString& tip, bool checkable = false) {
    return new IconButton(svgName, tip, checkable);
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);  // transparent margin so the shadow shows
    setFocusPolicy(Qt::StrongFocus);             // window takes keyboard focus (not the search box)
    resize(400, 550);

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

    settingsPanel_ = new SettingsPanel(this);
    helpPanel_ = new HelpPanel(this);
    stack_->addWidget(settingsPanel_);         // index 1
    stack_->addWidget(helpPanel_);             // index 2

    connect(settingsPanel_, &SettingsPanel::backRequested, this, &MainWindow::showList);
    connect(helpPanel_, &HelpPanel::backRequested, this, &MainWindow::showList);
    connect(settingsPanel_, &SettingsPanel::settingsChanged, this, &MainWindow::settingsChanged);
    connect(settingsPanel_, &SettingsPanel::checkUpdateRequested, this, &MainWindow::updateRequested);
    connect(settingsPanel_, &SettingsPanel::clearAllRequested, this, &MainWindow::clearAllRequested);

    // Preview popup + hover trigger
    preview_ = new PreviewPopup(this);
    hoverTimer_ = new QTimer(this);
    hoverTimer_->setSingleShot(true);
    hoverTimer_->setInterval(180);
    connect(hoverTimer_, &QTimer::timeout, this, [this] {
        if (hoverPreview_ && hoverRow_ >= 0 && isActiveWindow()) showPreviewRow(hoverRow_);
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
    list_->setItemDelegate(new RecordDelegate(this));
    list_->setUniformItemSizes(false);   // image rows vary in height
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setFocusPolicy(Qt::NoFocus);
    list_->viewport()->setAttribute(Qt::WA_Hover, true);   // deliver State_MouseOver to the delegate
    layout->addWidget(list_, 1);

    search_->installEventFilter(this);
    list_->installEventFilter(this);
    connect(search_, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    return page;
}

void MainWindow::setSettings(const AppSettings& s) {
    settingsPanel_->setSettings(s);
    hoverPreview_ = s.hoverPreview;
    spacePreview_ = s.spacePreview;
    previewLeft_ = (s.previewSide != "right");
    followCursor_ = (s.windowPlacement != "center");
}

void MainWindow::showPreviewRow(int row) {
    const ClipboardRecord* r = model_->recordAt(row);
    if (r) preview_->showPreview(*r, geometry().adjusted(16, 16, -16, -16), previewLeft_); // visible rect (drop shadow margin)
}

void MainWindow::hidePreview() {
    hoverTimer_->stop();
    if (preview_) preview_->hide();
}

bool MainWindow::pagePreview(Qt::MouseButton button) {
    if (!preview_ || !preview_->isVisible()) return false;
    if (button == Qt::BackButton)    { preview_->page(-1); return true; }  // M1 → previous page
    if (button == Qt::ForwardButton) { preview_->page(+1); return true; }  // M2 → next page
    return false;
}

void MainWindow::showList()     { stack_->setCurrentIndex(0); setFocus(); }
void MainWindow::showSettings() { stack_->setCurrentIndex(1); }
void MainWindow::showHelp()     { stack_->setCurrentIndex(2); }

QWidget* MainWindow::buildHeader() {
    auto* w = new QWidget(this);
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* title = new QLabel(QStringLiteral("hopy"), w);
    title->setObjectName("Title");
    h->addWidget(title);
    h->addStretch(1);
    // Update-check and About moved into the Settings panel; header keeps help + settings.
    auto* hlp = iconButton(QStringLiteral("help"),     QStringLiteral("快捷键说明"));
    auto* set = iconButton(QStringLiteral("settings"), QStringLiteral("设置"));
    connect(hlp, &QToolButton::clicked, this, &MainWindow::showHelp);
    connect(set, &QToolButton::clicked, this, &MainWindow::showSettings);
    for (auto* b : {hlp, set}) h->addWidget(b);
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
    auto* aa = inSearchToggle(QStringLiteral("Aa"), QStringLiteral("区分大小写"));
    auto* ww = inSearchToggle(QStringLiteral("W"), QStringLiteral("全词匹配"));
    auto* sl = new QHBoxLayout(search_);
    sl->setContentsMargins(14, 0, 8, 0);      // magnifier sits a little in from the left edge
    sl->setSpacing(2);
    sl->addWidget(mag);
    sl->addStretch(1);
    sl->addWidget(aa);
    sl->addWidget(ww);
    search_->setTextMargins(34, 0, 62, 0);    // clear the magnifier (left) and Aa/W (right)
    h->addWidget(search_, 1);
    h->addSpacing(2);

    // Type filters grouped in a rounded pill
    auto* filterGroup = new QFrame(w);
    filterGroup->setObjectName("FilterGroup");
    auto* fg = new QHBoxLayout(filterGroup);
    fg->setContentsMargins(3, 2, 3, 2);
    fg->setSpacing(2);
    btnText_  = iconButton(QStringLiteral("filter-text"),  QStringLiteral("文本"), true);
    btnImage_ = iconButton(QStringLiteral("filter-image"), QStringLiteral("图片"), true);
    btnFiles_ = iconButton(QStringLiteral("filter-files"), QStringLiteral("文件"), true);
    fg->addWidget(btnText_);
    fg->addWidget(btnImage_);
    fg->addWidget(btnFiles_);
    h->addWidget(filterGroup);

    // Favorites toggle in its own matching pill
    auto* favFrame = new QFrame(w);
    favFrame->setObjectName("FilterGroup");
    auto* ff = new QHBoxLayout(favFrame);
    ff->setContentsMargins(3, 2, 3, 2);
    btnFav_ = iconButton(QStringLiteral("filter-star"), QStringLiteral("仅收藏"), true);
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
    const auto mode = followCursor_ ? WindowPlacement::Cursor : WindowPlacement::Center;
    setGeometry(placeWindow(QCursor::pos(), geoms, size(), mode));
    search_->clear();
    updateFilterButtons();               // keep the last-used category (do not reset)
    applyFilter();
    show(); raise(); activateWindow();
    setFocus();   // window keeps keyboard focus for nav; search box not auto-focused (no caret blink)
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
            if (!search_->hasFocus()) { search_->setFocus(); return true; }
            return false;     // already in the search box: let "/" be typed
        case Qt::Key_Escape:  emit hideRequested(); return true;
        case Qt::Key_Delete:  { const qint64 id = currentId(); if (id) emit deleteRequested(id); return true; }
        default: break;
    }

    if (search_->text().isEmpty()) {
        switch (key) {
            case Qt::Key_F: { const qint64 id = currentId(); if (id) emit favoriteToggleRequested(id); return true; }
            case Qt::Key_D: { const qint64 id = currentId(); if (id) emit deleteRequested(id); return true; }
            case Qt::Key_T: { const qint64 id = currentId(); if (id) emit pinToggleRequested(id); return true; }
            case Qt::Key_1: case Qt::Key_2: case Qt::Key_3:
            case Qt::Key_4: case Qt::Key_5:
                confirmByIndex(key - Qt::Key_1); return true;
            default: break;
        }
    }
    return false;
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (handleNavKey(ke)) return true;
    }
    if (ev->type() == QEvent::MouseButtonPress) {
        // Mouse side buttons (M1/M2) page a visible preview instead of hitting the list.
        if (pagePreview(static_cast<QMouseEvent*>(ev)->button())) return true;
    }
    if (ev->type() == QEvent::Leave && obj == list_->viewport()) {
        hoverRow_ = -1;
        hidePreview();
    }
    return QWidget::eventFilter(obj, ev);
}

void MainWindow::keyPressEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat() && !search_->hasFocus()) {
        if (spacePreview_) showPreviewRow(list_->currentIndex().row());
        return;
    }
    if (!handleNavKey(ev)) QWidget::keyPressEvent(ev);
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev) {
    if (ev->key() == Qt::Key_Space && !ev->isAutoRepeat()) { hidePreview(); return; }
    QWidget::keyReleaseEvent(ev);
}

void MainWindow::hideEvent(QHideEvent* ev) {
    hidePreview();
    QWidget::hideEvent(ev);
}

void MainWindow::changeEvent(QEvent* ev) {
    if ((ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange) && searchIcon_) {
        searchIcon_->setPixmap(icons::svgPixmap(QStringLiteral("search"),
                                                palette().color(QPalette::Mid), 16));
    }
    if (ev->type() == QEvent::ActivationChange && isVisible() && !isActiveWindow()) {
        // Lost activation. Defer the check so our own popups (combobox/menu) or
        // panel switches don't count — only hide when focus really left the app.
        QTimer::singleShot(0, this, [this] {
            if (isVisible() && !isActiveWindow()
                && !QApplication::activePopupWidget()
                && !QApplication::activeWindow()) {
                hide();
            }
        });
    }
    QWidget::changeEvent(ev);
}

void MainWindow::mousePressEvent(QMouseEvent* ev) {
    if (pagePreview(ev->button())) return;   // M1/M2 over blank areas page the preview too
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
