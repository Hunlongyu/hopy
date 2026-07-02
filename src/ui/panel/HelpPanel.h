#pragma once
#include <QWidget>
namespace hopy {
class HelpPanel : public QWidget {
    Q_OBJECT
public:
    explicit HelpPanel(QWidget* parent = nullptr);
signals:
    void backRequested();
};
}
