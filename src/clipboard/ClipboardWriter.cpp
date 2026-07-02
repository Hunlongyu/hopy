#include "clipboard/ClipboardWriter.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QImage>
#include <QUrl>
#include <QFile>

namespace hopy {

void ClipboardWriter::writeText(const QString& text) {
    QGuiApplication::clipboard()->setText(text);
}

void ClipboardWriter::write(const ClipboardRecord& rec, bool plainTextOnly) {
    auto* mime = new QMimeData();
    switch (rec.type) {
        case ContentType::Text:
            mime->setText(rec.content);
            break;
        case ContentType::RichText:
            mime->setText(rec.content);
            if (!plainTextOnly && !rec.htmlPath.isEmpty()) {
                QFile f(rec.htmlPath);
                if (f.open(QIODevice::ReadOnly)) mime->setHtml(QString::fromUtf8(f.readAll()));
            }
            break;
        case ContentType::Image: {
            QImage img(rec.imagePath);
            if (!img.isNull()) mime->setImageData(img);
            else mime->setText(rec.content);
            break;
        }
        case ContentType::Files: {
            QList<QUrl> urls;
            for (const QString& p : rec.content.split('\n', Qt::SkipEmptyParts))
                urls << QUrl::fromLocalFile(p);
            mime->setUrls(urls);
            mime->setText(rec.content);
            break;
        }
    }
    QGuiApplication::clipboard()->setMimeData(mime);
}

} // namespace hopy
