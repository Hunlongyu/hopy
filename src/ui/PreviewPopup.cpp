#include "ui/PreviewPopup.h"
#include "util/I18n.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QPixmap>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>
#include <QFontMetrics>

namespace hopy {

PreviewPopup::PreviewPopup(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating);   // never steal focus
    setAttribute(Qt::WA_TranslucentBackground);   // let #PreviewCard round its corners

    auto* card = new QWidget(this);
    card->setObjectName("PreviewCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 12, 12, 12);
    scroll_ = new QScrollArea(card);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(false);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    content_ = new QLabel(scroll_);
    content_->setWordWrap(true);
    content_->setTextInteractionFlags(Qt::NoTextInteraction);
    content_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    QFont cf = content_->font();
    cf.setPixelSize(15);   // one px larger than the list card's 14px content text
    content_->setFont(cf);
    scroll_->setWidget(content_);
    lay->addWidget(scroll_);
}

void PreviewPopup::scrollByPixels(int dy) {
    if (auto* vb = scroll_->verticalScrollBar()) vb->setValue(vb->value() + dy);
}

void PreviewPopup::page(int dir) {
    // One viewport-worth minus a small overlap so nothing is skipped.
    if (auto* vb = scroll_->verticalScrollBar())
        vb->setValue(vb->value() + dir * qMax(1, vb->pageStep() - 16));
}

void PreviewPopup::showPreview(const ClipboardRecord& rec, const QRect& anchor, bool leftSide) {
    QScreen* scr = QGuiApplication::screenAt(anchor.center());
    const QRect sg = scr ? scr->availableGeometry() : QRect(0, 0, 1920, 1080);
    // Bounding box: max = main window size, min = one line of text.
    const int maxH = qMax(48, qMin(anchor.height(), sg.bottom() - anchor.top() - 8));
    const int maxContentW = qMax(160, anchor.width() - 24);   // window width cap minus margins

    int cw = qMin(360, maxContentW);   // content width
    int ch = 0;                        // content height (natural)

    // Clear the previous fixed size so a prior (long/image) preview can't leak
    // its dimensions into this measurement.
    content_->setMinimumSize(0, 0);
    content_->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    auto textHeight = [&](const QString& t) {
        // Deterministic wrapped-text height: same text + width => same height.
        const QRect br = QFontMetrics(content_->font())
            .boundingRect(QRect(0, 0, cw, 1'000'000), Qt::TextWordWrap, t);
        return br.height() + 4;
    };

    switch (rec.type) {
        case ContentType::Image: {
            QPixmap pm(rec.imagePath);
            if (!pm.isNull()) {
                // Native size; only downscale the WIDTH if it exceeds the box
                // (height overflow scrolls instead of blurring).
                QPixmap shown = pm.width() > maxContentW
                    ? pm.scaledToWidth(maxContentW, Qt::SmoothTransformation) : pm;
                content_->setText(QString());
                content_->setPixmap(shown);
                cw = shown.width();
                ch = shown.height();
            } else {
                content_->setPixmap(QPixmap());
                content_->setText(T("(image missing)"));
                ch = textHeight(content_->text());
            }
            break;
        }
        case ContentType::Files:
        case ContentType::Text:
        case ContentType::RichText: {
            content_->setPixmap(QPixmap());
            QString t = rec.content;
            if (t.size() > 8000) t = t.left(8000) + QStringLiteral("…");   // capped anyway; keeps it fast
            content_->setText(t);
            ch = textHeight(t);
            break;
        }
    }

    content_->setFixedSize(cw, qMax(1, ch));   // full content; scroll area shows a window into it
    const int w = cw + 24;
    const int h = qMin(ch + 24, maxH);
    resize(w, h);
    scroll_->verticalScrollBar()->setValue(0);

    // Fixed side (left by default), right edge adjacent to the main window's
    // left edge (or left edge adjacent to its right edge). Top-aligned.
    const int gap = 6;
    int x = leftSide ? anchor.left() - gap - w : anchor.right() + gap;
    x = qBound(sg.left(), x, sg.right() - w);
    int y = qBound(sg.top(), anchor.top(), sg.bottom() - h);
    move(x, y);
    show();
    raise();
}

} // namespace hopy
