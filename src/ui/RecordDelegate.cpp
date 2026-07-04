#include "ui/RecordDelegate.h"
#include "ui/RecordListModel.h"
#include "storage/Record.h"
#include "util/Icons.h"
#include "util/ColorDetect.h"
#include "util/TextInfo.h"
#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QPixmap>
#include <QImageReader>
#include <QFontMetrics>
#include <QFileInfo>
#include <QMouseEvent>

namespace hopy {

namespace { constexpr int kGlyphW = 22; }

QRect RecordDelegate::actionsRect(const QRect& itemRect) {
    const QRect card  = itemRect.adjusted(6, 3, -6, -3);       // kCardMarginX/Y
    const QRect inner = card.adjusted(12, 12 - 2, -12, -12 + 2); // kPad
    return QRect(inner.right() - kGlyphW * 3, inner.top(), kGlyphW * 3, 20);
}

int RecordDelegate::actionSlotAt(const QRect& itemRect, const QPoint& pos) {
    const QRect actions = actionsRect(itemRect);
    if (!actions.contains(pos)) return -1;
    const int slot = (pos.x() - actions.left()) / kGlyphW;
    return (slot >= 0 && slot <= 2) ? slot : -1;
}

bool RecordDelegate::editorEvent(QEvent* ev, QAbstractItemModel*,
                                 const QStyleOptionViewItem& opt, const QModelIndex& idx) {
    if (ev->type() != QEvent::MouseButtonRelease) return false;
    auto* me = static_cast<QMouseEvent*>(ev);
    if (me->button() != Qt::LeftButton) return false;
    const int slot = actionSlotAt(opt.rect, me->position().toPoint());
    if (slot < 0) return false;
    const qint64 id = idx.data(RecordListModel::IdRole).toLongLong();
    if (!id) return false;
    switch (slot) {
        case 0: emit favoriteClicked(id); break;
        case 1: emit pinClicked(id);      break;
        case 2: emit deleteClicked(id);   break;
    }
    return true;   // consume so the click doesn't also trigger row activation
}

namespace {
constexpr int kCardMarginX = 6;
constexpr int kCardMarginY = 3;
constexpr int kPad = 12;
constexpr int kRadius = 8;
constexpr int kTextRowHeight = 86;
constexpr int kFilesRowHeight = 104;
constexpr int kContentPx = 14;
constexpr int kPathPx = 12;
constexpr int kMaxThumbW = 200;
constexpr int kMaxThumbH = 100;
constexpr int kMetaH = 16;

QColor mix(const QColor& a, const QColor& b, double t) {
    return QColor(int(a.red()   * (1 - t) + b.red()   * t),
                  int(a.green() * (1 - t) + b.green() * t),
                  int(a.blue()  * (1 - t) + b.blue()  * t));
}

QString typeSvg(ContentType t) {
    switch (t) {
        case ContentType::Text:     return QStringLiteral("filter-text");
        case ContentType::RichText: return QStringLiteral("filter-text");
        case ContentType::Image:    return QStringLiteral("filter-image");
        case ContentType::Files:    return QStringLiteral("filter-files");
    }
    return QStringLiteral("filter-text");
}
} // namespace

QSize RecordDelegate::thumbSizeFor(const QString& path) const {
    auto it = thumbCache_.find(path);
    if (it != thumbCache_.end()) return it.value();

    QSize result(0, 0);
    QImageReader reader(path);
    const QSize s = reader.size();
    if (s.isValid() && s.width() > 0 && s.height() > 0) {
        double k = qMin(double(kMaxThumbW) / s.width(), double(kMaxThumbH) / s.height());
        k = qMin(k, 1.0);
        result = QSize(qMax(1, int(s.width() * k)), qMax(1, int(s.height() * k)));
    }
    thumbCache_.insert(path, result);
    return result;
}

void RecordDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = QApplication::palette();
    const bool selected = opt.state & QStyle::State_Selected;
    const QColor cardColor = pal.color(QPalette::Base);
    const QColor textColor = pal.color(QPalette::Text);
    const QColor subColor  = pal.color(QPalette::Mid);
    const QColor accent    = pal.color(QPalette::Highlight);
    const QColor cardBorder = pal.color(QPalette::Midlight);

    const QRect card = opt.rect.adjusted(kCardMarginX, kCardMarginY, -kCardMarginX, -kCardMarginY);
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    QColor fillColor = cardColor;
    if (opt.state & QStyle::State_MouseOver) {
        fillColor = cardColor.lightnessF() < 0.5 ? QColor(0x4b, 0x4b, 0x4b)   // dark hover
                                                 : QColor(0xe8, 0xed, 0xf8);  // light hover (Ropy list_active)
    }
    p->fillPath(path, fillColor);
    if (selected) {
        QColor sb = textColor; sb.setAlpha(150);
        p->setPen(QPen(sb, 1.5));
    } else {
        p->setPen(QPen(cardBorder, 1.0));
    }
    p->drawPath(path);

    const auto type = static_cast<ContentType>(idx.data(RecordListModel::TypeRole).toInt());
    const bool pinned   = idx.data(RecordListModel::PinnedRole).toBool();
    const bool favorite = idx.data(RecordListModel::FavoriteRole).toBool();
    const QRect inner = card.adjusted(kPad, kPad - 2, -kPad, -kPad + 2);

    // Right-side action icons (favorite / pin / delete)
    const int isz = 15;
    const QRect actions = actionsRect(opt.rect);
    auto drawIcon = [&](int slot, const QString& name, const QColor& col) {
        QRect cell(actions.left() + kGlyphW * slot, actions.top(), kGlyphW, 20);
        QRect ir(0, 0, isz, isz); ir.moveCenter(cell.center());
        p->drawPixmap(ir, icons::svgPixmap(name, col, isz));
    };
    drawIcon(0, QStringLiteral("filter-star"), favorite ? accent : subColor);
    drawIcon(1, QStringLiteral("pin-to-top"), pinned ? accent : subColor);
    drawIcon(2, QStringLiteral("circle-x"), subColor);

    // Meta text (index + timestamp) + type marker at far right
    QFont mf = opt.font; mf.setPointSizeF(opt.font.pointSizeF() - 1);
    const int num = idx.row() + 1;
    const qint64 ms = idx.data(RecordListModel::CreatedAtRole).toLongLong();
    const QString ts = relativeTime(ms, QDateTime::currentMSecsSinceEpoch());
    const QString meta = QString::number(num) + "   " + ts;
    auto drawMeta = [&](const QRect& metaRect) {
        p->setFont(mf);
        p->setPen(subColor);
        p->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter, meta);
        QRect mk(0, 0, 13, 13);
        mk.moveRight(metaRect.right());
        mk.moveTop(metaRect.center().y() - 6);
        p->drawPixmap(mk, icons::svgPixmap(typeSvg(type), subColor, 13));   // type marker for EVERY type
    };

    const int rightLimit = actions.left() - 8;

    if (type == ContentType::Image) {
        const QString imgPath = idx.data(RecordListModel::ImagePathRole).toString();
        QSize t = thumbSizeFor(imgPath);
        if (t.isEmpty()) t = QSize(64, 48);
        const QRect thumb(inner.left(), inner.top(), t.width(), t.height());
        QPixmap pm(imgPath);
        if (!pm.isNull()) {
            QPainterPath clip; clip.addRoundedRect(thumb, 4, 4);
            p->save(); p->setClipPath(clip);
            p->drawPixmap(thumb, pm.scaled(thumb.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            p->restore();
        } else {
            p->fillRect(thumb, mix(cardColor, textColor, 0.1));
            QRect ic(0, 0, 22, 22); ic.moveCenter(thumb.center());
            p->drawPixmap(ic, icons::svgPixmap(QStringLiteral("filter-image"), subColor, 22));
        }
        drawMeta(QRect(inner.left(), thumb.bottom() + 4, inner.width(), kMetaH));
        p->restore();
        return;
    }

    if (type == ContentType::Files) {
        const QString raw = idx.data(RecordListModel::RawRole).toString();
        const QString firstPath = raw.section('\n', 0, 0);
        QString name = QFileInfo(firstPath).fileName();
        if (name.isEmpty()) name = firstPath;

        // Line 1: file name, elide in the MIDDLE (keeps prefix + extension)
        QFont nf = opt.font; nf.setPixelSize(kContentPx);
        p->setFont(nf); p->setPen(textColor);
        const int nameH = QFontMetrics(nf).lineSpacing();
        const QRect nameRect(inner.left(), inner.top(), rightLimit - inner.left(), nameH);
        p->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(nf).elidedText(name, Qt::ElideMiddle, nameRect.width()));

        // Line 2-3: full path, up to 2 lines, tail clipped (no ellipsis)
        QFont pf = opt.font; pf.setPixelSize(kPathPx);
        p->setFont(pf); p->setPen(subColor);
        const int pathLineH = QFontMetrics(pf).lineSpacing();
        const QRect pathRect(inner.left(), nameRect.bottom() + 2, rightLimit - inner.left(), pathLineH * 2);
        QTextOption pto(Qt::AlignLeft | Qt::AlignTop);
        pto.setWrapMode(QTextOption::WrapAnywhere);   // fill lines; don't orphan "E:" of a path
        p->save(); p->setClipRect(pathRect);
        p->drawText(pathRect, firstPath, pto);
        p->restore();

        drawMeta(QRect(inner.left(), inner.bottom() - kMetaH, inner.width(), kMetaH));
        p->restore();
        return;
    }

    // Text / rich-text: content capped at exactly 2 lines
    QFont cf = opt.font; cf.setPixelSize(kContentPx);
    p->setFont(cf);
    const int lineH = QFontMetrics(cf).lineSpacing();
    QRect contentRect(inner.left(), inner.top(), rightLimit - inner.left(), lineH * 2);

    // Colour swatch when the content holds a colour value (#hex / rgb / rgba / bare hex)
    const QColor swatch = detectColor(idx.data(RecordListModel::RawRole).toString());
    if (swatch.isValid()) {
        const int s = 14;
        QRect sr(contentRect.left(), contentRect.top() + qMax(0, (lineH - s) / 2), s, s);
        QPainterPath sp; sp.addRoundedRect(sr, 3, 3);
        p->fillPath(sp, swatch);
        p->setPen(QPen(QColor(128, 128, 128, 120), 1));
        p->drawPath(sp);
        contentRect.setLeft(sr.right() + 7);
    }

    p->setPen(textColor);
    QTextOption to(Qt::AlignLeft | Qt::AlignTop);
    // Fill each line before wrapping. WrapAtWordBoundaryOrAnywhere treats ':' as a
    // break point, so a path like "E:\..." wraps right after "E:" — WrapAnywhere
    // avoids that orphan and keeps long no-space strings (paths, URLs) tidy.
    to.setWrapMode(QTextOption::WrapAnywhere);
    p->save(); p->setClipRect(contentRect);
    p->drawText(contentRect, idx.data(Qt::DisplayRole).toString(), to);
    p->restore();

    drawMeta(QRect(inner.left(), inner.bottom() - kMetaH, inner.width(), kMetaH));
    p->restore();
}

QSize RecordDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex& idx) const {
    const auto type = static_cast<ContentType>(idx.data(RecordListModel::TypeRole).toInt());
    if (type == ContentType::Image) {
        const QSize t = thumbSizeFor(idx.data(RecordListModel::ImagePathRole).toString());
        const int th = t.height() > 0 ? t.height() : 56;
        return QSize(0, th + kMetaH + 30);
    }
    if (type == ContentType::Files) return QSize(0, kFilesRowHeight);
    return QSize(0, kTextRowHeight);
}

} // namespace hopy
