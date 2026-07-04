#include "util/TextInfo.h"
#include "util/I18n.h"
#include <QDateTime>

namespace hopy {

int charCount(const QString& text) { return static_cast<int>(text.toUcs4().size()); }

int lineCount(const QString& text) {
    if (text.isEmpty()) return 0;
    return static_cast<int>(text.count(QLatin1Char('\n'))) + 1;
}

QString relativeTime(qint64 msecs, qint64 nowMsecs) {
    const qint64 diff = (nowMsecs - msecs) / 1000;      // seconds; negative => clock skew
    if (diff < 60)   return T("just now");
    if (diff < 3600) return T("%1 min ago").arg(diff / 60);

    const QDateTime t   = QDateTime::fromMSecsSinceEpoch(msecs);
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMsecs);
    if (t.date() == now.date())            return T("%1 h ago").arg(diff / 3600);
    if (t.date() == now.date().addDays(-1)) return T("Yesterday %1").arg(t.toString("HH:mm"));
    if (t.date().year() == now.date().year()) return t.toString("MM-dd");
    return t.toString("yyyy-MM-dd");
}

} // namespace hopy
