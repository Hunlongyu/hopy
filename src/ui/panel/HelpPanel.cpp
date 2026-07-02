#include "ui/panel/HelpPanel.h"
#include "util/Icons.h"
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
    auto* title = new QLabel(QStringLiteral("快捷键说明"));
    title->setObjectName("PanelTitle");
    head->addWidget(back, 0, 0, Qt::AlignLeft);
    head->addWidget(title, 0, 0, Qt::AlignCenter);
    root->addLayout(head);

    struct Row { const char* key; const char* desc; };
    const Row rows[] = {
        {"/", "聚焦搜索框"},
        {"↑ / ↓", "选择上一项 / 下一项"},
        {"← / → / Tab", "切换内容类型（循环）"},
        {"1 – 5", "快速选中第 N 项"},
        {"Enter", "确认选中项"},
        {"Shift+Enter", "以纯文本确认选中项"},
        {"F", "切换收藏"},
        {"T", "切换置顶"},
        {"Delete / D", "删除选中项"},
        {"Esc", "隐藏窗口"},
        {"M4 / M5", "预览翻页（鼠标侧键 M4 下翻 / M5 上翻）"},
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
        auto* d = new QLabel(QString::fromUtf8(row.desc));
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
