#pragma once
#include <QString>
namespace hopy::paths {
QString dataDir();       // <AppDataLocation>/hopy
QString imagesDir();     // dataDir/images
QString richTextDir();   // dataDir/rich_text
QString ensureDir(const QString& path);
}
