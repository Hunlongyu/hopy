#include "ui/OpenAction.h"
#include "util/OpenTarget.h"
#include "platform/RevealInExplorer.h"
#include <QDesktopServices>
#include <QUrl>

namespace hopy {

bool openRecord(const ClipboardRecord& rec) {
    if (rec.type == ContentType::Files) {
        const QString first = rec.content.section(QLatin1Char('\n'), 0, 0).trimmed();
        return platform::revealInExplorer(first);
    }
    if (rec.type == ContentType::Text || rec.type == ContentType::RichText) {
        const OpenTarget t = detectOpenTarget(rec.content);
        switch (t.kind) {
            case OpenKind::Url:   return QDesktopServices::openUrl(QUrl(t.value));
            case OpenKind::Email: return QDesktopServices::openUrl(QUrl(QStringLiteral("mailto:") + t.value));
            case OpenKind::Path:  return platform::revealInExplorer(t.value);
            case OpenKind::None:  return false;
        }
    }
    return false;
}

} // namespace hopy
