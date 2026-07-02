#pragma once
#include <QWidget>
#include "storage/Record.h"
#include "ui/TypeFilter.h"
#include "config/Settings.h"

class QLineEdit;
class QListView;
class QToolButton;
class QLabel;
class QStackedWidget;
class QTimer;

namespace hopy {

class RecordListModel;
class SettingsPanel;
class HelpPanel;
class AboutPanel;
class PreviewPopup;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void setRecords(const QList<ClipboardRecord>& records);
    void setSettings(const AppSettings& s);
    void showAtCursor();

signals:
    void confirmRequested(qint64 id, bool plainText);
    void pinToggleRequested(qint64 id);
    void favoriteToggleRequested(qint64 id);
    void deleteRequested(qint64 id);
    void hideRequested();
    void settingsChanged(const AppSettings& s);
    void updateRequested();
    void clearAllRequested();

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;      // release Space to hide preview
    void mousePressEvent(QMouseEvent* ev) override;    // drag window from non-interactive areas
    void changeEvent(QEvent* ev) override;             // auto-hide when the window loses activation
    void hideEvent(QHideEvent* ev) override;           // hide preview when the window hides
    bool focusNextPrevChild(bool) override { return false; } // let us handle Tab

private:
    QWidget* buildListPage();
    QWidget* buildHeader();
    QWidget* buildSearchFilterRow();
    void showList();
    void showSettings();
    void showHelp();
    void applyFilter();
    void updateFilterButtons();
    void moveSelection(int delta);
    void showPreviewRow(int row);
    void hidePreview();
    bool pagePreview(Qt::MouseButton button);   // M1/M2 side buttons page the preview
    void setFilter(ContentFilter f);
    void confirmCurrent(bool plainText);
    void confirmByIndex(int index);
    qint64 currentId() const;
    bool handleNavKey(QKeyEvent* ev);

    QStackedWidget* stack_ = nullptr;
    SettingsPanel* settingsPanel_ = nullptr;
    HelpPanel* helpPanel_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* searchIcon_ = nullptr;   // magnifier (re-tinted on theme change)
    QListView* list_ = nullptr;
    RecordListModel* model_ = nullptr;
    QToolButton* btnText_ = nullptr;
    QToolButton* btnImage_ = nullptr;
    QToolButton* btnFiles_ = nullptr;
    QToolButton* btnFav_ = nullptr;

    ContentFilter filter_ = ContentFilter::All;   // remembered across shows

    PreviewPopup* preview_ = nullptr;
    QTimer* hoverTimer_ = nullptr;
    int hoverRow_ = -1;
    bool hoverPreview_ = true;
    bool spacePreview_ = true;
    bool previewLeft_ = true;
    bool followCursor_ = true;   // window placement: follow cursor vs screen centre
};

} // namespace hopy
