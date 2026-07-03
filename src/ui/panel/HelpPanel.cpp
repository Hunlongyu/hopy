#include "ui/panel/HelpPanel.h"
#include "util/Icons.h"
#include "util/I18n.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>

namespace hopy {

HelpPanel::HelpPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 12);
    root->setSpacing(10);

    // Header: back (left) + centered title
    auto* head = new QGridLayout();
    auto* back = new QToolButton();
    back->setFont(icons::iconFont(12));
    back->setText(icons::ch(0xE72B));
    back->setAutoRaise(true);
    back->setCursor(Qt::PointingHandCursor);
    back->setFocusPolicy(Qt::NoFocus);
    connect(back, &QToolButton::clicked, this, &HelpPanel::backRequested);
    auto* title = new QLabel(T("Shortcuts"));
    title->setObjectName("PanelTitle");
    head->addWidget(back, 0, 0, Qt::AlignLeft);
    head->addWidget(title, 0, 0, Qt::AlignCenter);
    root->addLayout(head);

    struct Row { const char* key; const char* desc; };  // desc is an English key for T()
    const Row rows[] = {
        {"/", "Focus the search box"},
        {"↑ / ↓", "Select previous / next"},
        {"← / → / Tab", "Cycle content type"},
        {"1 – 5", "Pick item N"},
        {"Enter", "Confirm selection"},
        {"Shift+Enter", "Confirm as plain text"},
        {"F", "Toggle favorite"},
        {"T", "Toggle pin"},
        {"Delete / D", "Delete selection"},
        {"Esc", "Hide window"},
        {"M4 / M5", "Preview paging (mouse M4 down / M5 up)"},
    };

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);
    int r = 0;
    for (const Row& row : rows) {
        auto* k = new QLabel(QString::fromUtf8(row.key));
        k->setObjectName("KeyCap");
        k->setAlignment(Qt::AlignCenter);
        k->setFixedHeight(28);
        grid->addWidget(k, r, 0);
        auto* d = new QLabel(T(row.desc));
        grid->addWidget(d, r, 1, Qt::AlignVCenter);
        ++r;
    }

    // Center the table both horizontally and vertically
    auto* tableHost = new QWidget();
    auto* th = new QHBoxLayout(tableHost);
    th->setContentsMargins(0, 0, 0, 0);
    th->addLayout(grid);

    root->addStretch(1);
    auto* centerRow = new QHBoxLayout();
    centerRow->addStretch(1);
    centerRow->addWidget(tableHost);
    centerRow->addStretch(1);
    root->addLayout(centerRow);
    root->addStretch(1);
}

} // namespace hopy
