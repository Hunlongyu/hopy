#include "ui/panel/AboutPanel.h"
#include "util/Icons.h"
#include "util/I18n.h"
#include "util/Version.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace hopy {

AboutPanel::AboutPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(10);

    auto* head = new QHBoxLayout();
    auto* back = new QToolButton();
    back->setFont(icons::iconFont(12));
    back->setText(icons::ch(0xE72B));
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    connect(back, &QToolButton::clicked, this, &AboutPanel::backRequested);
    auto* title = new QLabel(T("About"));
    title->setObjectName("PanelTitle");
    head->addWidget(back);
    head->addSpacing(6);
    head->addWidget(title);
    head->addStretch(1);
    root->addLayout(head);

    root->addStretch(1);
    auto* logo = new QLabel();
    logo->setFont(icons::iconFont(48));
    logo->setText(icons::ch(0xE8C8)); // Copy glyph as logo
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    auto* name = new QLabel(QStringLiteral("Hopy"));
    name->setObjectName("Title");
    name->setAlignment(Qt::AlignCenter);
    root->addWidget(name);

    auto* ver = new QLabel(T("Version %1").arg(currentVersion()));
    ver->setAlignment(Qt::AlignCenter);
    root->addWidget(ver);

    auto* desc = new QLabel(T("A lightweight clipboard manager built with Qt 6 and SQLite."));
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    root->addWidget(desc);

    auto* lic = new QLabel(T("MIT License"));
    lic->setAlignment(Qt::AlignCenter);
    root->addWidget(lic);
    root->addStretch(2);
}

} // namespace hopy
