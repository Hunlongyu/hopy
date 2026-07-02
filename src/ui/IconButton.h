#pragma once
#include <QToolButton>
#include <QString>

namespace hopy {

// A tool button whose SVG icon is re-rendered with the current theme colours
// whenever the palette/style changes — so light/dark switching never leaves a
// stale (invisible) icon baked from the old theme.
class IconButton : public QToolButton {
    Q_OBJECT
public:
    IconButton(const QString& svgName, const QString& tip, bool checkable = false,
               QWidget* parent = nullptr);

protected:
    void changeEvent(QEvent* ev) override;

private:
    void rebuildIcon();
    QString svg_;
};

} // namespace hopy
