#pragma once
#include <QWidget>
#include "config/Settings.h"

class QComboBox;
class QCheckBox;
class QSpinBox;
class QSlider;
class QPushButton;

namespace hopy {

class HotkeyEdit;

class SettingsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);
    void setSettings(const AppSettings& s);
    void setUpdateAvailable(const QString& tag);   // reflect a pending update on the check button

signals:
    void backRequested();
    void settingsChanged(const AppSettings& s);
    void checkUpdateRequested();
    void clearAllRequested();

private:
    void emitChange();

    AppSettings current_;
    bool loading_ = false;
    QComboBox* language_ = nullptr;
    QComboBox* theme_ = nullptr;
    QComboBox* placement_ = nullptr;
    QComboBox* previewSide_ = nullptr;
    HotkeyEdit* hotkey_ = nullptr;
    QCheckBox* pasteImmediate_ = nullptr;
    QCheckBox* hover_ = nullptr;
    QCheckBox* space_ = nullptr;
    QCheckBox* autostart_ = nullptr;
    QCheckBox* fullscreenBlock_ = nullptr;
    QCheckBox* maskSensitive_ = nullptr;
    QComboBox* openMouse_ = nullptr;   // right / middle / off
    QComboBox* openKey_ = nullptr;     // single-key picker: Off / A..Z
    QSpinBox* maxHistory_ = nullptr;
    QSpinBox* maxStorage_ = nullptr;
    QSlider* opacity_ = nullptr;
    QPushButton* checkBtn_ = nullptr;
};

} // namespace hopy
