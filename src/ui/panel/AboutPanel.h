#pragma once
#include <QWidget>
namespace hopy {
class AboutPanel : public QWidget {
    Q_OBJECT
public:
    explicit AboutPanel(QWidget* parent = nullptr);
signals:
    void backRequested();
};
}
