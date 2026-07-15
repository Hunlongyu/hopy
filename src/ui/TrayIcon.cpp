#include "ui/TrayIcon.h"
#include "util/I18n.h"
#include "util/Icons.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QApplication>
#include <QPalette>
#include <QSignalBlocker>
#include <QProxyStyle>
#include <QStyleOption>
#include <QPointF>

namespace hopy {

namespace {

// Bump the menu's icon size so our padded icons render 1:1 (no downscaling squish).
class MenuStyle : public QProxyStyle {
public:
    int pixelMetric(PixelMetric metric, const QStyleOption* opt = nullptr,
                    const QWidget* w = nullptr) const override {
        if (metric == PM_SmallIconSize) return 22;
        return QProxyStyle::pixelMetric(metric, opt, w);
    }
};

// QMenu anchors the item icon to the highlight's left edge and ignores
// QMenu::item's padding-left for it, so bake a transparent left gap into the
// pixmap: draw the glyph onto a wider (22px, matching MenuStyle) canvas.
QPixmap menuPixmap(const QString& name, const QColor& c) {
    constexpr int box = 22, glyph = 14, padL = 8;
    const qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    QPixmap canvas(qRound(box * dpr), qRound(box * dpr));
    canvas.fill(Qt::transparent);
    canvas.setDevicePixelRatio(dpr);
    QPainter p(&canvas);
    p.drawPixmap(QPointF(padL, (box - glyph) / 2.0), icons::svgPixmap(name, c, glyph));
    p.end();
    return canvas;
}

QIcon menuIcon(const QString& name, const QColor& c) { return QIcon(menuPixmap(name, c)); }

} // namespace

TrayIcon::TrayIcon(QObject* parent) : QObject(parent) {
    baseIcon_ = QIcon(QStringLiteral(":/logo.ico"));
    tray_ = new QSystemTrayIcon(baseIcon_, this);

    menu_ = new QMenu();
    menu_->setObjectName("TrayMenu");
    menu_->setAttribute(Qt::WA_TranslucentBackground);   // clean rounded corners (bg comes from QSS)
    auto* menuStyle = new MenuStyle();
    menuStyle->setParent(menu_);                         // outlive with the menu
    menu_->setStyle(menuStyle);

    showAct_  = menu_->addAction(T("Show main window"));
    checkAct_ = menu_->addAction(T("Check for updates"));
    setAct_   = menu_->addAction(T("Settings"));
    menu_->addSeparator();
    pauseAct_ = menu_->addAction(T("Pause monitoring"));
    pauseAct_->setCheckable(true);
    autostartAct_ = menu_->addAction(T("Launch at startup"));
    autostartAct_->setCheckable(true);
    menu_->addSeparator();
    quitAct_  = menu_->addAction(T("Quit"));

    connect(showAct_,      &QAction::triggered, this, &TrayIcon::showRequested);
    connect(checkAct_,     &QAction::triggered, this, &TrayIcon::checkUpdateRequested);
    connect(setAct_,       &QAction::triggered, this, &TrayIcon::settingsRequested);
    connect(pauseAct_, &QAction::toggled, this, [this](bool on) {
        syncToggle(pauseAct_, QStringLiteral("pause"), "Pause monitoring");   // restyle before the menu closes
        emit pauseToggled(on);
    });
    connect(autostartAct_, &QAction::toggled, this, [this](bool on) {
        syncToggle(autostartAct_, QStringLiteral("power"), "Launch at startup");
        emit autostartToggled(on);
    });
    connect(quitAct_,      &QAction::triggered, this, &TrayIcon::quitRequested);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick)
            emit showRequested();
    });

    rebuildIcons();
    tray_->setContextMenu(menu_);
    tray_->setToolTip(QStringLiteral("Hopy\n") + T("Clipboard manager"));
    tray_->show();
}

void TrayIcon::rebuildIcons() {
    const QColor c = qApp->palette().color(QPalette::Mid);   // muted, per-theme icon colour
    showAct_->setIcon(menuIcon(QStringLiteral("layout-grid"), c));
    checkAct_->setIcon(menuIcon(QStringLiteral("refresh"), c));
    setAct_->setIcon(menuIcon(QStringLiteral("settings"), c));
    quitAct_->setIcon(menuIcon(QStringLiteral("log-out"), QColor(0xe5, 0x53, 0x4b)));   // red
    syncToggle(pauseAct_, QStringLiteral("pause"), "Pause monitoring");
    syncToggle(autostartAct_, QStringLiteral("power"), "Launch at startup");
}

// A QSS-styled QMenu with a custom item icon paints no native check mark, so signal the
// state ourselves — twice, so it's unmistakable: a trailing ✓ on the label when on, plus
// an accent glyph (muted when off). Rebuilt whenever the state, theme, or language changes.
void TrayIcon::syncToggle(QAction* act, const QString& iconName, const char* baseText) {
    const bool on = act->isChecked();
    const QColor off = qApp->palette().color(QPalette::Mid);   // matches every idle row
    const QColor accent(0x6b, 0x8c, 0xff);                     // hopy accent
    act->setIcon(menuIcon(iconName, on ? accent : off));
    act->setText(on ? T(baseText) + QStringLiteral("   ✓") : T(baseText));
}

void TrayIcon::applyTheme() { rebuildIcons(); }

void TrayIcon::retranslate() {
    showAct_->setText(T("Show main window"));
    checkAct_->setText(T("Check for updates"));
    setAct_->setText(T("Settings"));
    syncToggle(pauseAct_, QStringLiteral("pause"), "Pause monitoring");           // keep the ✓ across a language switch
    syncToggle(autostartAct_, QStringLiteral("power"), "Launch at startup");
    quitAct_->setText(T("Quit"));
    tray_->setToolTip(QStringLiteral("Hopy\n") + T("Clipboard manager"));
}

void TrayIcon::setPaused(bool on) {
    QSignalBlocker block(pauseAct_);        // reflect state without re-emitting toggled
    pauseAct_->setChecked(on);
    syncToggle(pauseAct_, QStringLiteral("pause"), "Pause monitoring");   // blocker skips toggled, so restyle here
}

void TrayIcon::setAutostart(bool on) {
    QSignalBlocker block(autostartAct_);
    autostartAct_->setChecked(on);
    syncToggle(autostartAct_, QStringLiteral("power"), "Launch at startup");
}

void TrayIcon::setUpdateBadge(bool on) {
    if (!on) { tray_->setIcon(baseIcon_); return; }
    const int s = 64;
    QPixmap pm = baseIcon_.pixmap(s, s);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const int d = 22;                                    // red dot diameter
    const QRect dot(s - d - 2, s - d - 2, d, d);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xff, 0xff, 0xff));                // white ring for contrast on any tray
    p.drawEllipse(dot.adjusted(-3, -3, 3, 3));
    p.setBrush(QColor(0xe5, 0x53, 0x4b));                // red badge
    p.drawEllipse(dot);
    p.end();
    tray_->setIcon(QIcon(pm));
}

} // namespace hopy
