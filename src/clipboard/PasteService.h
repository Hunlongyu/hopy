#pragma once
#include <QObject>
#include <functional>
#include "storage/Record.h"
#include "config/Settings.h"

namespace hopy {

class PasteService : public QObject {
    Q_OBJECT
public:
    explicit PasteService(QObject* parent = nullptr);
    void confirm(const ClipboardRecord& rec, ConfirmMode mode, bool plainText,
                 std::function<void()> onWritten);

signals:
    void hideWindowRequested();
};

} // namespace hopy
