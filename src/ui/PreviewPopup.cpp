#include "ui/PreviewPopup.h"
#include "util/I18n.h"
#include "util/TextInfo.h"
#include "util/OpenTarget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QPixmap>
#include <QScreen>
#include <QGuiApplication>
#include <QFont>
#include <QFontMetrics>
#include <QDateTime>
#include <QStringList>

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
    lay->setSpacing(6);   // fixed gap between the info bar and the scroll area (used in showPreview's height calc)
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

    info_ = new QLabel(card);
    info_->setObjectName("PreviewInfo");
    info_->setWordWrap(false);
    pct_ = new QLabel(card);
    pct_->setObjectName("PreviewInfo");   // same muted styling as the meta line
    pct_->setWordWrap(false);
    pct_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    {
        QFont f = info_->font();
        f.setPixelSize(11);
        info_->setFont(f);
        pct_->setFont(f);
    }
    auto* infoRow = new QHBoxLayout();
    infoRow->setContentsMargins(0, 0, 0, 0);
    infoRow->setSpacing(8);
    infoRow->addWidget(info_, 1);   // meta line takes the width; percentage hugs the right
    infoRow->addWidget(pct_, 0);
    lay->addLayout(infoRow);         // sits above the scroll area

    lay->addWidget(scroll_);

    // Live scroll progress from any source (momentum, wheel, keyboard).
    if (auto* vb = scroll_->verticalScrollBar()) {
        connect(vb, &QScrollBar::valueChanged, this, [this] { updateScrollPercent(); });
        connect(vb, &QScrollBar::rangeChanged,  this, [this] { updateScrollPercent(); });
    }
}

void PreviewPopup::updateScrollPercent() {
    auto* vb = scroll_->verticalScrollBar();
    const int max = vb ? vb->maximum() : 0;
    if (max <= 0) { pct_->clear(); return; }   // content fits → nothing to scroll
    pct_->setText(QStringLiteral("%1%").arg(qRound(100.0 * vb->value() / max)));
}

void PreviewPopup::scrollByPixels(int dy) {
    if (auto* vb = scroll_->verticalScrollBar()) vb->setValue(vb->value() + dy);
}

void PreviewPopup::setOpenKeysLabel(const QString& label) { openKeysLabel_ = label; }

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

    // A password-manager-flagged secret is masked in the preview too (Text only);
    // counts and the open hint are suppressed so nothing about it leaks.
    const bool masked = maskSensitive_ && rec.sensitive &&
                        (rec.type == ContentType::Text || rec.type == ContentType::RichText);
    const QString body = masked ? maskSecret(rec.content) : rec.content;

    // Top info bar: type · counts · absolute time · open hint.
    const QString typeLabel =
        rec.type == ContentType::Image ? T("Image")
      : rec.type == ContentType::Files ? T("Files")
                                       : T("Text");
    const QString absTime =
        QDateTime::fromMSecsSinceEpoch(rec.createdAt).toString("yyyy-MM-dd HH:mm:ss");
    QStringList parts{typeLabel};
    if (!masked && (rec.type == ContentType::Text || rec.type == ContentType::RichText))
        parts << T("%1 chars").arg(charCount(rec.content))
              << T("%1 lines").arg(lineCount(rec.content));
    parts << absTime;

    QString verb;
    if (rec.type == ContentType::Files) {
        verb = T("reveal in file manager");
    } else if (!masked && (rec.type == ContentType::Text || rec.type == ContentType::RichText)) {
        switch (detectOpenTarget(rec.content).kind) {
            case OpenKind::Url:   verb = T("open link"); break;
            case OpenKind::Email: verb = T("open email"); break;
            case OpenKind::Path:  verb = T("reveal in file manager"); break;
            case OpenKind::None:  break;
        }
    }
    if (!verb.isEmpty() && !openKeysLabel_.isEmpty())
        parts << (openKeysLabel_ + QStringLiteral(" ") + verb);
    info_->setText(parts.join(QStringLiteral("  ·  ")));

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
            QString t = body;
            if (t.size() > 8000) t = t.left(8000) + QStringLiteral("…");   // capped anyway; keeps it fast
            content_->setText(t);
            ch = textHeight(t);
            break;
        }
    }

    content_->setFixedSize(cw, qMax(1, ch));   // full content; scroll area shows a window into it
    const int w = cw + 24;
    // Height must budget for the info bar above the scroll area, else it eats
    // into the content viewport and short text gets clipped to nothing.
    const int infoH = info_->sizeHint().height();
    const int h = qMin(ch + 24 + infoH + 6, maxH);   // +infoH + the 6px layout spacing
    resize(w, h);
    scroll_->verticalScrollBar()->setValue(0);
    updateScrollPercent();   // reset the indicator for the new content (0% or blank if it fits)

    // Two candidate positions — beside the main window on the LEFT or the RIGHT.
    // Use the preferred side (leftSide) when it fits on screen; otherwise flip to
    // the opposite side instead of clamping on top of the main window. Only fall
    // back to clamping when neither side can hold it (very small screen).
    const int gap = 6;
    const int xLeft  = anchor.left()  - gap - w;   // preview sits left of the window
    const int xRight = anchor.right() + gap;        // preview sits right of the window
    const bool leftFits  = xLeft  >= sg.left();
    const bool rightFits = xRight + w <= sg.right();
    bool useLeft = leftSide;
    if ( leftSide && !leftFits  && rightFits) useLeft = false;   // preferred left won't fit, right will
    if (!leftSide && !rightFits && leftFits ) useLeft = true;    // preferred right won't fit, left will
    const int x = qBound(sg.left(), useLeft ? xLeft : xRight, sg.right() - w);
    const int y = qBound(sg.top(), anchor.top(), sg.bottom() - h);
    move(x, y);
    show();
    raise();
}

} // namespace hopy
