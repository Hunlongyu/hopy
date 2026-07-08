#include "ui/Theme.h"
#include <QApplication>
#include <QStyleHints>
#include <QPalette>
#include <QColor>
#include <QFile>

namespace hopy {

namespace {

QPalette darkPalette() {
    // Matches Ropy "Ropy Dark": neutral grey surfaces + #6b8cff accent.
    QPalette p;
    const QColor window(0x2d, 0x2d, 0x2d);    // background
    const QColor base(0x3c, 0x3c, 0x3c);      // secondary / cards
    const QColor text(0xff, 0xff, 0xff);      // foreground
    const QColor sub(0x88, 0x88, 0x88);       // muted_foreground
    const QColor highlight(0x6b, 0x8c, 0xff); // primary
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::PlaceholderText, sub);
    p.setColor(QPalette::Disabled, QPalette::Text, sub);
    p.setColor(QPalette::Mid, sub);
    p.setColor(QPalette::Link, highlight);                   // rich-text <a> links (About card)
    p.setColor(QPalette::Midlight, QColor(0x4c, 0x4c, 0x4c)); // card border
    return p;
}

QPalette lightPalette() {
    QPalette p;
    // Matches Ropy "Ropy Light": white surface, #f5f5f5 cards, #5b7bd5 accent.
    const QColor window(0xff, 0xff, 0xff);    // background
    const QColor base(0xf5, 0xf5, 0xf5);      // secondary / cards
    const QColor text(0x1a, 0x1a, 0x1a);      // foreground
    const QColor sub(0x6b, 0x6b, 0x6b);       // muted_foreground
    const QColor highlight(0x5b, 0x7b, 0xd5); // primary
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, base);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::PlaceholderText, sub);
    p.setColor(QPalette::Disabled, QPalette::Text, sub);
    p.setColor(QPalette::Mid, sub);
    p.setColor(QPalette::Link, highlight);                   // rich-text <a> links (About card)
    p.setColor(QPalette::Midlight, QColor(0xe0, 0xe0, 0xe0)); // card border
    return p;
}

} // namespace

void applyTheme(const QString& name) {
    // "auto" follows the OS colour scheme (Qt 6.5+); explicit dark/light override it.
    bool light;
    if (name == "light")     light = true;
    else if (name == "dark") light = false;
    else                     light = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Light);
    qApp->setPalette(light ? lightPalette() : darkPalette());

    QFile f(light ? ":/themes/light.qss" : ":/themes/dark.qss");
    if (f.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    else
        qWarning("hopy: theme qss %s not found", qPrintable(name));
}

} // namespace hopy
