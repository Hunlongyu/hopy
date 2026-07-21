#include <QtTest>
#include <QPlainTextEdit>
#include <QScrollBar>
#include "ui/PreviewPopup.h"

using namespace hopy;

class TestPreviewPopup : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        qApp->setStyleSheet(QStringLiteral(
            "QScrollBar:vertical, QScrollBar:horizontal {"
            "width: 0px; height: 0px; background: transparent; }"));
    }

    void wrappedFilePathFitsWithoutScrolling() {
        PreviewPopup popup;
        ClipboardRecord record;
        record.type = ContentType::Files;
        record.content = QStringLiteral(
            "C:/Users/hunlo/AppData/Local/PixPin/Temp/PixPin_2026-07-20_16-26-08.png");

        popup.showPreview(record, QRect(100, 100, 368, 504), false);
        QCoreApplication::processEvents();

        auto* editor = popup.findChild<QPlainTextEdit*>();
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->document()->blockCount(), 1);

        QTextCursor endCursor = editor->textCursor();
        endCursor.movePosition(QTextCursor::End);
        const QRect endRect = editor->cursorRect(endCursor);
        QVERIFY2(endRect.bottom() <= editor->viewport()->height(),
                 qPrintable(QStringLiteral("document end %1 exceeds viewport height %2")
                                .arg(endRect.bottom())
                                .arg(editor->viewport()->height())));
    }

    void wrappedSingleLineTextFitsWithoutScrolling() {
        PreviewPopup popup;
        ClipboardRecord record;
        record.type = ContentType::Text;
        record.content = QStringLiteral(
            "dps://?purl=https%3A%2F%2Fpages-fast.m.taobao.com%2Fwow%2F")
                             .leftJustified(617, QLatin1Char('x'));

        popup.showPreview(record, QRect(100, 100, 368, 504), false);
        QCoreApplication::processEvents();

        auto* editor = popup.findChild<QPlainTextEdit*>();
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->document()->blockCount(), 1);

        QTextCursor endCursor = editor->textCursor();
        endCursor.movePosition(QTextCursor::End);
        const QRect endRect = editor->cursorRect(endCursor);
        QVERIFY2(endRect.bottom() <= editor->viewport()->height(),
                 qPrintable(QStringLiteral("document end %1 exceeds viewport height %2")
                                .arg(endRect.bottom())
                                .arg(editor->viewport()->height())));
    }

    void ctrlWheelMovesLessThanOnePage() {
        PreviewPopup popup;
        ClipboardRecord record;
        record.type = ContentType::Text;
        for (int i = 0; i < 500; ++i)
            record.content += QStringLiteral("preview line %1\n").arg(i);

        popup.showPreview(record, QRect(100, 100, 368, 504), false);
        QCoreApplication::processEvents();

        auto* editor = popup.findChild<QPlainTextEdit*>();
        QVERIFY(editor != nullptr);

        auto* scrollBar = editor->verticalScrollBar();
        QVERIFY(scrollBar->maximum() > scrollBar->pageStep());
        const int before = scrollBar->value();
        popup.scrollByPixels(120);
        QCoreApplication::processEvents();

        const int moved = scrollBar->value() - before;
        QVERIFY2(moved > 0, "Ctrl + wheel must scroll the preview downward");
        QVERIFY2(moved < scrollBar->pageStep(),
                 qPrintable(QStringLiteral("one wheel notch moved %1 lines; page step is %2")
                                .arg(moved)
                                .arg(scrollBar->pageStep())));
    }
};

QTEST_MAIN(TestPreviewPopup)
#include "test_preview_popup.moc"
