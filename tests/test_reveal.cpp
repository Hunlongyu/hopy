#include <QtTest>
#include "platform/RevealInExplorer.h"
using namespace hopy::platform;

class TestReveal : public QObject {
    Q_OBJECT
private slots:
    void nonexistentPathReturnsFalse() {
        QVERIFY(!revealInExplorer("Z:/definitely/not/here_9f8a7b"));
    }
    void emptyPathReturnsFalse() {
        QVERIFY(!revealInExplorer(QString()));
    }
};
QTEST_APPLESS_MAIN(TestReveal)
#include "test_reveal.moc"
