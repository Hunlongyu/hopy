#include "ui/panel/SettingsPanel.h"
#include "ui/IconButton.h"
#include "ui/HotkeyEdit.h"
#include "util/Icons.h"
#include "util/I18n.h"
#include "util/Version.h"
#include "update/UpdateConfig.h"
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QStyle>

namespace hopy {

namespace {
// A titled rounded card; returns the card and hands back its content layout.
QFrame* makeCard(const QString& title, QVBoxLayout*& inner) {
    auto* card = new QFrame();
    card->setObjectName("SettingCard");
    inner = new QVBoxLayout(card);
    inner->setContentsMargins(14, 10, 14, 6);
    inner->setSpacing(0);
    auto* t = new QLabel(title);
    t->setObjectName("CardTitle");
    inner->addWidget(t);
    inner->addSpacing(2);
    return card;
}

// One "label ........ control" row, with an optional thin divider under it.
void addRow(QVBoxLayout* card, const QString& label, QWidget* control, bool divider) {
    auto* row = new QWidget();
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 9, 0, 9);
    rl->addWidget(new QLabel(label));
    rl->addStretch(1);
    rl->addWidget(control);
    card->addWidget(row);
    if (divider) {
        auto* line = new QFrame();
        line->setObjectName("RowDivider");
        line->setFixedHeight(1);
        card->addWidget(line);
    }
}
} // namespace

SettingsPanel::SettingsPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(10);

    // Header: back (left) + centered title (grid overlay in one cell)
    auto* head = new QGridLayout();
    auto* back = new QToolButton();
    back->setFont(icons::iconFont(12));
    back->setText(icons::ch(0xE72B));
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    connect(back, &QToolButton::clicked, this, &SettingsPanel::backRequested);
    auto* title = new QLabel(T("Settings"));
    title->setObjectName("PanelTitle");
    head->addWidget(back, 0, 0, Qt::AlignLeft);
    head->addWidget(title, 0, 0, Qt::AlignCenter);
    root->addLayout(head);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget();
    auto* col = new QVBoxLayout(content);
    col->setContentsMargins(0, 0, 4, 0);
    col->setSpacing(12);

    // Controls
    language_ = new QComboBox();
    language_->addItem(T("Auto"), "auto");
    language_->addItem(QStringLiteral("中文"), "zh");
    language_->addItem(QStringLiteral("English"), "en");
    language_->setFixedWidth(120);
    theme_ = new QComboBox();
    theme_->addItem(T("Dark"), "dark");
    theme_->addItem(T("Light"), "light");
    theme_->setFixedWidth(120);
    placement_ = new QComboBox();
    placement_->addItem(T("Mouse position"), "cursor");
    placement_->addItem(T("Screen center"), "center");
    placement_->setFixedWidth(120);
    previewSide_ = new QComboBox();
    previewSide_->addItem(T("Left"), "left");
    previewSide_->addItem(T("Right"), "right");
    previewSide_->setFixedWidth(120);
    opacity_ = new QSlider(Qt::Horizontal);
    opacity_->setRange(60, 100);   // min 60% so the window never becomes too transparent to fix
    opacity_->setFixedWidth(160);
    hotkey_ = new HotkeyEdit();
    hotkey_->setFixedWidth(160);
    pasteImmediate_ = new QCheckBox();
    hover_ = new QCheckBox();
    space_ = new QCheckBox();
    autostart_ = new QCheckBox();
    fullscreenBlock_ = new QCheckBox();
    maskSensitive_ = new QCheckBox();
    openMouse_ = new QComboBox();
    openMouse_->addItem(T("Right-click"), "right");
    openMouse_->addItem(T("Middle-click"), "middle");
    openMouse_->addItem(T("Off"), "none");
    openMouse_->setFixedWidth(120);
    openKey_ = new QComboBox();
    openKey_->addItem(T("Off"), "");
    // Skip letters already bound to in-panel actions — D=delete, F=favorite,
    // T=pin-to-top — since those win over the open key and would make it a no-op.
    for (char c = 'A'; c <= 'Z'; ++c) {
        if (c == 'D' || c == 'F' || c == 'T') continue;
        openKey_->addItem(QString(QChar(c)), QString(QChar(c)));
    }
    openKey_->setFixedWidth(120);
    maxHistory_ = new QSpinBox(); maxHistory_->setRange(10, 1000); maxHistory_->setFixedWidth(110);
    maxStorage_ = new QSpinBox(); maxStorage_->setRange(10, 1000); maxStorage_->setFixedWidth(110);

    QVBoxLayout* gn; auto* gnCard = makeCard(T("General"), gn);
    addRow(gn, T("Language"), language_, true);
    addRow(gn, T("Theme"), theme_, true);
    addRow(gn, T("Opacity"), opacity_, true);
    addRow(gn, T("Start on boot"), autostart_, false);
    col->addWidget(gnCard);

    QVBoxLayout* ac; auto* acCard = makeCard(T("Activation"), ac);
    addRow(ac, T("Activation hotkey"), hotkey_, true);
    addRow(ac, T("Open with mouse"), openMouse_, true);
    addRow(ac, T("Open with key"), openKey_, true);
    addRow(ac, T("Suppress hotkey in fullscreen"), fullscreenBlock_, false);
    col->addWidget(acCard);

    QVBoxLayout* pv; auto* pvCard = makeCard(T("Panel & Preview"), pv);
    addRow(pv, T("Fallback position"), placement_, true);
    addRow(pv, T("Hover preview"), hover_, true);
    addRow(pv, T("Space preview"), space_, true);
    addRow(pv, T("Preview side"), previewSide_, false);
    col->addWidget(pvCard);

    QVBoxLayout* pp; auto* ppCard = makeCard(T("Paste & Privacy"), pp);
    addRow(pp, T("Paste immediately on confirm"), pasteImmediate_, true);
    addRow(pp, T("Mask sensitive content"), maskSensitive_, false);
    col->addWidget(ppCard);

    QVBoxLayout* st; auto* stCard = makeCard(T("Storage"), st);
    addRow(st, T("Items shown"), maxHistory_, true);
    addRow(st, T("Storage limit"), maxStorage_, true);
    auto* clearBtn = new QPushButton(T("Clear all"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#c0392b;color:#fff;border:none;}"
        "QPushButton:hover{background:#a93226;}"));
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        QMessageBox box(QMessageBox::Question, T("Clear history"),
                        T("Clear all clipboard records? This cannot be undone."),
                        QMessageBox::NoButton, this);
        auto* yes = box.addButton(T("Yes"), QMessageBox::YesRole);
        box.addButton(T("No"), QMessageBox::NoRole);
        box.exec();
        if (box.clickedButton() == yes) emit clearAllRequested();
    });
    addRow(st, T("Clear history"), clearBtn, false);
    col->addWidget(stCard);

    QVBoxLayout* ab; auto* abCard = makeCard(T("About"), ab);
    // Hero: app name + version pill on one line, tagline underneath. No logo — we
    // don't have our own mark yet.
    auto* nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(8);
    auto* name = new QLabel(QStringLiteral("Hopy"));
    name->setStyleSheet(QStringLiteral("font-weight:600;font-size:16px;"));
    auto* verPill = new QLabel(QStringLiteral("v") + currentVersion());
    verPill->setObjectName("VersionPill");
    nameRow->addWidget(name);
    nameRow->addWidget(verPill);
    nameRow->addStretch(1);
    ab->addLayout(nameRow);
    ab->addSpacing(3);
    auto* desc = new QLabel(T("A lightweight clipboard manager."));
    desc->setObjectName("AboutMuted");
    desc->setWordWrap(true);
    ab->addWidget(desc);
    ab->addSpacing(10);
    auto* abLine = new QFrame();
    abLine->setObjectName("RowDivider");
    abLine->setFixedHeight(1);
    ab->addWidget(abLine);
    ab->addSpacing(10);
    // Meta grid: muted label | value, neatly aligned in two columns.
    auto* meta = new QGridLayout();
    meta->setContentsMargins(0, 0, 0, 0);
    meta->setVerticalSpacing(8);
    meta->setHorizontalSpacing(18);
    meta->setColumnStretch(1, 1);
    auto addMeta = [&](int r, const QString& k, QWidget* v) {
        auto* kl = new QLabel(k);
        kl->setObjectName("AboutMuted");
        meta->addWidget(kl, r, 0, Qt::AlignLeft | Qt::AlignVCenter);
        meta->addWidget(v, r, 1, Qt::AlignLeft | Qt::AlignVCenter);
    };
    // Author → GitHub profile. A plain QLabel link is unreliable in hopy's
    // never-activated window, so use a clickable button with the exact same
    // clicked -> openUrl mechanism as the GitHub button below, styled as a link.
    auto* author = new QToolButton();
    author->setText(QStringLiteral("Hunlongyu ↗"));
    author->setToolButtonStyle(Qt::ToolButtonTextOnly);
    author->setObjectName("AuthorLink");
    author->setAutoRaise(true);
    author->setCursor(Qt::PointingHandCursor);
    author->setFocusPolicy(Qt::NoFocus);
    connect(author, &QToolButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(update::ownerUrl())); });
    addMeta(0, T("Author"), author);
    addMeta(1, T("Tech"), new QLabel(QStringLiteral("Qt 6 · SQLite")));
    addMeta(2, T("License"), new QLabel(QStringLiteral("MIT License")));
    addMeta(3, T("Copyright"), new QLabel(QStringLiteral("© 2026 Hunlongyu")));
    ab->addLayout(meta);
    ab->addSpacing(12);
    auto* aboutRow = new QWidget();
    auto* arl = new QHBoxLayout(aboutRow);
    arl->setContentsMargins(0, 0, 0, 0);
    checkBtn_ = new QPushButton(T("Check for updates"));
    checkBtn_->setObjectName("CheckUpdateBtn");
    checkBtn_->setCursor(Qt::PointingHandCursor);
    connect(checkBtn_, &QPushButton::clicked, this, &SettingsPanel::checkUpdateRequested);
    auto* gh = new IconButton(QStringLiteral("github"), T("View on GitHub"), false);
    connect(gh, &QToolButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(update::repoUrl())); });
    arl->addWidget(checkBtn_);
    arl->addStretch(1);
    arl->addWidget(gh);
    ab->addWidget(aboutRow);
    col->addWidget(abCard);

    col->addStretch(1);
    scroll->setWidget(content);
    root->addWidget(scroll);

    connect(language_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(theme_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(placement_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(previewSide_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(opacity_, &QSlider::valueChanged, this, [this] { emitChange(); });
    connect(hotkey_, &HotkeyEdit::keySequenceChanged, this, [this] { emitChange(); });
    for (QCheckBox* c : {pasteImmediate_, hover_, space_, autostart_, fullscreenBlock_, maskSensitive_})
        connect(c, &QCheckBox::toggled, this, [this] { emitChange(); });
    connect(openMouse_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(openKey_,   &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(maxHistory_, &QSpinBox::valueChanged, this, [this] { emitChange(); });
    connect(maxStorage_, &QSpinBox::valueChanged, this, [this] { emitChange(); });
}

void SettingsPanel::setSettings(const AppSettings& s) {
    loading_ = true;
    current_ = s;
    language_->setCurrentIndex(s.language == "zh" ? 1 : s.language == "en" ? 2 : 0);
    theme_->setCurrentIndex(s.theme == "light" ? 1 : 0);
    placement_->setCurrentIndex(s.windowPlacement == "center" ? 1 : 0);
    previewSide_->setCurrentIndex(s.previewSide == "right" ? 1 : 0);
    opacity_->setValue(s.windowOpacity);
    hotkey_->setKeySequence(QKeySequence(s.hotkey));
    pasteImmediate_->setChecked(s.confirmMode == ConfirmMode::PasteImmediately);
    hover_->setChecked(s.hoverPreview);
    space_->setChecked(s.spacePreview);
    autostart_->setChecked(s.autostart);
    fullscreenBlock_->setChecked(s.suppressOnFullscreen);
    maskSensitive_->setChecked(s.maskSensitive);
    openMouse_->setCurrentIndex(qMax(0, openMouse_->findData(s.openMouseButton)));
    openKey_->setCurrentIndex(qMax(0, openKey_->findData(s.openKey)));
    maxHistory_->setValue(s.maxHistory);
    maxStorage_->setValue(s.maxStorage);
    loading_ = false;
}

void SettingsPanel::setUpdateAvailable(const QString& tag) {
    if (!checkBtn_) return;
    const bool has = !tag.isEmpty();
    checkBtn_->setText(has ? T("New version %1").arg(tag) : T("Check for updates"));
    checkBtn_->setProperty("hasUpdate", has);
    checkBtn_->style()->unpolish(checkBtn_);   // re-evaluate the [hasUpdate] style
    checkBtn_->style()->polish(checkBtn_);
}

void SettingsPanel::emitChange() {
    if (loading_) return;
    AppSettings s = current_;
    s.language = language_->currentData().toString();
    s.theme = theme_->currentData().toString();
    s.windowPlacement = placement_->currentData().toString();
    s.previewSide = previewSide_->currentData().toString();
    s.windowOpacity = opacity_->value();
    s.hotkey = hotkey_->keySequence().toString();   // may be empty = no global hotkey
    s.confirmMode = pasteImmediate_->isChecked() ? ConfirmMode::PasteImmediately : ConfirmMode::CopyOnly;
    s.hoverPreview = hover_->isChecked();
    s.spacePreview = space_->isChecked();
    s.autostart = autostart_->isChecked();
    s.suppressOnFullscreen = fullscreenBlock_->isChecked();
    s.maskSensitive = maskSensitive_->isChecked();
    s.openMouseButton = openMouse_->currentData().toString();
    s.openKey = openKey_->currentData().toString();
    s.maxHistory = maxHistory_->value();
    s.maxStorage = maxStorage_->value();
    current_ = s;
    emit settingsChanged(s);
}

} // namespace hopy
