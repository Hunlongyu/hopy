#include "util/Icons.h"
#include <QPainter>
#include <QPixmap>
#include <QHash>
#include <QGuiApplication>
#include <QSvgRenderer>

namespace hopy::icons {

QFont iconFont(int pointSize) {
    QFont f;
    f.setFamilies({QStringLiteral("Segoe Fluent Icons"), QStringLiteral("Segoe MDL2 Assets")});
    if (pointSize > 0) f.setPointSize(pointSize);
    return f;
}

QString ch(char16_t code) { return QString(QChar(code)); }

QIcon glyphIcon(char16_t code, const QColor& color, int px) {
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QFont f = iconFont();
    f.setPixelSize(static_cast<int>(px * 0.9));
    p.setFont(f);
    p.setPen(color);
    p.drawText(pm.rect(), Qt::AlignCenter, ch(code));
    return QIcon(pm);
}

QPixmap svgPixmap(const QString& name, const QColor& color, int px) {
    static QHash<QString, QPixmap> cache;
    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    const QString key = name + '|' + color.name(QColor::HexArgb) + '|' +
                        QString::number(px) + '|' + QString::number(dpr);
    if (auto it = cache.constFind(key); it != cache.constEnd()) return it.value();

    const int phys = qMax(1, int(px * dpr));
    QPixmap pm(phys, phys);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    {
        QSvgRenderer renderer(QStringLiteral(":/icons/%1.svg").arg(name));
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, px, px));
        // Tint: keep alpha shape, replace colour.
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(QRectF(0, 0, px, px), color);
    }
    cache.insert(key, pm);
    return pm;
}

QIcon svgIcon(const QString& name, const QColor& color, int px) {
    return QIcon(svgPixmap(name, color, px));
}

QIcon svgToggleIcon(const QString& name, const QColor& off, const QColor& on, int px) {
    QIcon ic;
    ic.addPixmap(svgPixmap(name, off, px), QIcon::Normal, QIcon::Off);
    ic.addPixmap(svgPixmap(name, on, px), QIcon::Normal, QIcon::On);
    return ic;
}

} // namespace hopy::icons
