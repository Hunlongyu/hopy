#pragma once
#include <QString>

namespace hopy {

enum class ContentType { Text = 0, RichText = 1, Image = 2, Files = 3 };

struct ClipboardRecord {
    qint64 id = 0;              // content hash (u64 bit-cast to i64); dedup primary key
    QString content;           // plain text / summary / file paths (\n-joined)
    ContentType type = ContentType::Text;
    qint64 createdAt = 0;      // epoch millis
    bool pinned = false;
    bool favorite = false;
    QString htmlPath;          // RichText HTML sidecar, empty if none
    QString rtfPath;           // RichText RTF sidecar
    QString imagePath;         // Image PNG file path
    bool sensitive = false;    // OS-flagged secret (e.g. a password); masked in the UI
};

// Mask a secret for display: keep a couple of leading chars, then a fixed run of
// dots (fixed length so the real length isn't leaked). Full content is stored and
// pasted unchanged; only the on-screen text is masked.
inline QString maskSecret(const QString& s) {
    QString first = s;
    const int nl = first.indexOf('\n');
    if (nl >= 0) first.truncate(nl);
    return first.left(2) + QString(6, QChar(0x2022));   // e.g. "Xk••••••"
}

} // namespace hopy
