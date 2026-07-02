#include "util/Paths.h"
#include <QStandardPaths>
#include <QDir>

namespace hopy::paths {

QString dataDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
QString ensureDir(const QString& path) {
    QDir().mkpath(path);
    return path;
}
QString imagesDir()   { return ensureDir(dataDir() + "/images"); }
QString richTextDir() { return ensureDir(dataDir() + "/rich_text"); }

} // namespace hopy::paths
