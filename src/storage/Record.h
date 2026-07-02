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
};

} // namespace hopy
