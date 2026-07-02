#pragma once
#include <QObject>
#include <memory>
class QHotkey;
namespace hopy {
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;  // out-of-line: unique_ptr<QHotkey> needs complete type
    bool setShortcut(const QString& sequence);
signals:
    void activated();
private:
    std::unique_ptr<QHotkey> hotkey_;
};
}
