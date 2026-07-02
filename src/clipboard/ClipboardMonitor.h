#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QImage>

namespace hopy {

enum class PayloadKind { None, Files, RichText, Image, Text };

struct CapturedPayload {
    PayloadKind kind = PayloadKind::None;
    QString text;
    QString html;
    QImage image;
    QStringList files;
};

PayloadKind preferredKind(bool hasFiles, bool hasRichText, bool hasImage, bool hasText);
bool shouldForwardText(const QString& last, const QString& next);
bool shouldForwardHash(quint64 last, quint64 next);

class ClipboardMonitor : public QObject {
    Q_OBJECT
public:
    explicit ClipboardMonitor(QObject* parent = nullptr);
    void start();
    // Called when we write to the clipboard ourselves, so the echo isn't re-saved.
    void setSelfCopyText(const QString& text) { lastText_ = text; }

signals:
    void payloadCaptured(const CapturedPayload& payload);

private slots:
    void onClipboardChanged();

private:
    QString lastText_;
    quint64 lastImageHash_ = 0;
    quint64 lastFilesHash_ = 0;
};

} // namespace hopy
