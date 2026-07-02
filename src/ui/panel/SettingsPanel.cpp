#include "ui/panel/SettingsPanel.h"
#include "ui/IconButton.h"
#include "ui/HotkeyEdit.h"
#include "util/Icons.h"
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
    auto* title = new QLabel(QStringLiteral("设置"));
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
    theme_ = new QComboBox();
    theme_->addItem(QStringLiteral("深色"), "dark");
    theme_->addItem(QStringLiteral("浅色"), "light");
    theme_->setFixedWidth(120);
    placement_ = new QComboBox();
    placement_->addItem(QStringLiteral("跟随光标"), "cursor");
    placement_->addItem(QStringLiteral("屏幕中央"), "center");
    placement_->setFixedWidth(120);
    previewSide_ = new QComboBox();
    previewSide_->addItem(QStringLiteral("左侧"), "left");
    previewSide_->addItem(QStringLiteral("右侧"), "right");
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
    maxHistory_ = new QSpinBox(); maxHistory_->setRange(10, 1000); maxHistory_->setFixedWidth(110);
    maxStorage_ = new QSpinBox(); maxStorage_->setRange(10, 1000); maxStorage_->setFixedWidth(110);

    QVBoxLayout* ap; auto* apCard = makeCard(QStringLiteral("外观"), ap);
    addRow(ap, QStringLiteral("主题"), theme_, true);
    addRow(ap, QStringLiteral("显示位置"), placement_, true);
    addRow(ap, QStringLiteral("预览位置"), previewSide_, true);
    addRow(ap, QStringLiteral("不透明度"), opacity_, false);
    col->addWidget(apCard);

    QVBoxLayout* bh; auto* bhCard = makeCard(QStringLiteral("行为"), bh);
    addRow(bh, QStringLiteral("激活快捷键"), hotkey_, true);
    addRow(bh, QStringLiteral("确认后立即粘贴"), pasteImmediate_, true);
    addRow(bh, QStringLiteral("悬停预览"), hover_, true);
    addRow(bh, QStringLiteral("空格预览"), space_, true);
    addRow(bh, QStringLiteral("开机自启"), autostart_, false);
    col->addWidget(bhCard);

    QVBoxLayout* st; auto* stCard = makeCard(QStringLiteral("存储"), st);
    addRow(st, QStringLiteral("显示条数"), maxHistory_, true);
    addRow(st, QStringLiteral("存储上限"), maxStorage_, true);
    auto* clearBtn = new QPushButton(QStringLiteral("清空全部"));
    clearBtn->setCursor(Qt::PointingHandCursor);
    clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#c0392b;color:#fff;border:none;}"
        "QPushButton:hover{background:#a93226;}"));
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("清空历史"),
                QStringLiteral("确定清空全部剪贴板记录吗？此操作不可撤销。"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            emit clearAllRequested();
    });
    addRow(st, QStringLiteral("清空历史"), clearBtn, false);
    col->addWidget(stCard);

    QVBoxLayout* ab; auto* abCard = makeCard(QStringLiteral("关于"), ab);
    auto* nameVer = new QLabel(QStringLiteral("hopy　·　版本 0.1.0"));
    nameVer->setStyleSheet(QStringLiteral("font-weight:600;"));
    ab->addWidget(nameVer);
    ab->addSpacing(4);
    auto* desc = new QLabel(QStringLiteral("使用 Qt 6 与 SQLite 构建的轻量剪贴板管理器。"));
    desc->setWordWrap(true);
    ab->addWidget(desc);
    ab->addSpacing(2);
    ab->addWidget(new QLabel(QStringLiteral("MIT 开源协议")));
    ab->addSpacing(8);
    auto* aboutRow = new QWidget();
    auto* arl = new QHBoxLayout(aboutRow);
    arl->setContentsMargins(0, 0, 0, 0);
    auto* checkBtn = new QPushButton(QStringLiteral("检查更新"));
    checkBtn->setCursor(Qt::PointingHandCursor);
    connect(checkBtn, &QPushButton::clicked, this, &SettingsPanel::checkUpdateRequested);
    auto* gh = new IconButton(QStringLiteral("github"), QStringLiteral("GitHub"), false);
    connect(gh, &QToolButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com"))); });
    arl->addWidget(checkBtn);
    arl->addStretch(1);
    arl->addWidget(gh);
    ab->addWidget(aboutRow);
    col->addWidget(abCard);

    col->addStretch(1);
    scroll->setWidget(content);
    root->addWidget(scroll);

    connect(theme_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(placement_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(previewSide_, &QComboBox::currentIndexChanged, this, [this] { emitChange(); });
    connect(opacity_, &QSlider::valueChanged, this, [this] { emitChange(); });
    connect(hotkey_, &HotkeyEdit::keySequenceChanged, this, [this] { emitChange(); });
    for (QCheckBox* c : {pasteImmediate_, hover_, space_, autostart_})
        connect(c, &QCheckBox::toggled, this, [this] { emitChange(); });
    connect(maxHistory_, &QSpinBox::valueChanged, this, [this] { emitChange(); });
    connect(maxStorage_, &QSpinBox::valueChanged, this, [this] { emitChange(); });
}

void SettingsPanel::setSettings(const AppSettings& s) {
    loading_ = true;
    current_ = s;
    theme_->setCurrentIndex(s.theme == "light" ? 1 : 0);
    placement_->setCurrentIndex(s.windowPlacement == "center" ? 1 : 0);
    previewSide_->setCurrentIndex(s.previewSide == "right" ? 1 : 0);
    opacity_->setValue(s.windowOpacity);
    hotkey_->setKeySequence(QKeySequence(s.hotkey));
    pasteImmediate_->setChecked(s.confirmMode == ConfirmMode::PasteImmediately);
    hover_->setChecked(s.hoverPreview);
    space_->setChecked(s.spacePreview);
    autostart_->setChecked(s.autostart);
    maxHistory_->setValue(s.maxHistory);
    maxStorage_->setValue(s.maxStorage);
    loading_ = false;
}

void SettingsPanel::emitChange() {
    if (loading_) return;
    AppSettings s = current_;
    s.theme = theme_->currentData().toString();
    s.windowPlacement = placement_->currentData().toString();
    s.previewSide = previewSide_->currentData().toString();
    s.windowOpacity = opacity_->value();
    s.hotkey = hotkey_->keySequence().toString();   // may be empty = no global hotkey
    s.confirmMode = pasteImmediate_->isChecked() ? ConfirmMode::PasteImmediately : ConfirmMode::CopyOnly;
    s.hoverPreview = hover_->isChecked();
    s.spacePreview = space_->isChecked();
    s.autostart = autostart_->isChecked();
    s.maxHistory = maxHistory_->value();
    s.maxStorage = maxStorage_->value();
    current_ = s;
    emit settingsChanged(s);
}

} // namespace hopy
