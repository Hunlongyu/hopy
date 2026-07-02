#include "clipboard/ClipboardMonitor.h"
#include "util/Hash.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>

namespace hopy {

PayloadKind preferredKind(bool hasFiles, bool hasRichText, bool hasImage, bool hasText) {
    if (hasFiles)    return PayloadKind::Files;
    if (hasRichText) return PayloadKind::RichText;
    if (hasImage)    return PayloadKind::Image;
    if (hasText)     return PayloadKind::Text;
    return PayloadKind::None;
}

bool shouldForwardText(const QString& last, const QString& next) { return last != next; }
bool shouldForwardHash(quint64 last, quint64 next) { return last != next; }

ClipboardMonitor::ClipboardMonitor(QObject* parent) : QObject(parent) {}

void ClipboardMonitor::start() {
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged,
            this, &ClipboardMonitor::onClipboardChanged);
}

void ClipboardMonitor::onClipboardChanged() {
    const QMimeData* m = QGuiApplication::clipboard()->mimeData();
    if (!m) return;

    const bool hasFiles = m->hasUrls() && [&] {
        for (const QUrl& u : m->urls()) if (u.isLocalFile()) return true;
        return false;
    }();
    const bool hasText  = m->hasText();
    const bool hasRich  = hasText && m->hasHtml();
    const bool hasImage = m->hasImage();

    CapturedPayload p;
    p.kind = preferredKind(hasFiles, hasRich, hasImage, hasText);

    switch (p.kind) {
        case PayloadKind::Files: {
            QStringList files;
            for (const QUrl& u : m->urls()) if (u.isLocalFile()) files << u.toLocalFile();
            const quint64 h = Hash::contentHash(ContentType::Files, files.join('\n').toUtf8());
            if (!shouldForwardHash(lastFilesHash_, h)) return;
            lastFilesHash_ = h;
            p.files = files;
            break;
        }
        case PayloadKind::RichText: {
            p.text = m->text();
            if (!shouldForwardText(lastText_, p.text)) return;
            lastText_ = p.text;
            p.html = m->html();
            break;
        }
        case PayloadKind::Image: {
            p.image = qvariant_cast<QImage>(m->imageData());
            if (p.image.isNull()) return;
            const QByteArray bytes(reinterpret_cast<const char*>(p.image.constBits()),
                                   static_cast<int>(p.image.sizeInBytes()));
            const quint64 h = Hash::fnv1a(bytes);
            if (!shouldForwardHash(lastImageHash_, h)) return;
            lastImageHash_ = h;
            break;
        }
        case PayloadKind::Text: {
            p.text = m->text();
            if (!shouldForwardText(lastText_, p.text)) return;
            lastText_ = p.text;
            break;
        }
        case PayloadKind::None:
            return;
    }
    emit payloadCaptured(p);
}

} // namespace hopy
