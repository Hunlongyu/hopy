#include <QtTest>
#include "update/UpdateChecker.h"
using namespace hopy;

// NOTE: plain (escaped) string literals, never R"(...)" — moc's lexer in this Qt
// build misparses the "//" inside "https://" in a raw string and emits an empty
// .moc, breaking the link (see test_update_parse.cpp).

static QByteArray sampleAtom() {
    // A GitHub releases.atom with a pre-release entry first, then the stable one.
    return QByteArray(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<feed xmlns=\"http://www.w3.org/2005/Atom\">\n"
        "  <title>Release notes from hopy</title>\n"
        "  <entry>\n"
        "    <id>tag:github.com,2008:Repository/1/v0.3.0-beta</id>\n"
        "    <link rel=\"alternate\" type=\"text/html\" href=\"https://github.com/Hunlongyu/hopy/releases/tag/v0.3.0-beta\"/>\n"
        "    <title>v0.3.0 beta</title>\n"
        "    <content type=\"html\">&lt;p&gt;beta preview&lt;/p&gt;</content>\n"
        "  </entry>\n"
        "  <entry>\n"
        "    <id>tag:github.com,2008:Repository/1/v0.2.0</id>\n"
        "    <link rel=\"alternate\" type=\"text/html\" href=\"https://github.com/Hunlongyu/hopy/releases/tag/v0.2.0\"/>\n"
        "    <title>v0.2.0</title>\n"
        "    <content type=\"html\">&lt;ul&gt;&lt;li&gt;fixed a bug&lt;/li&gt;&lt;/ul&gt;</content>\n"
        "  </entry>\n"
        "</feed>\n");
}

class TestUpdateNotes : public QObject {
    Q_OBJECT
private slots:
    void picksEntryMatchingTag() {
        const QString notes = notesFromAtom(sampleAtom(), QStringLiteral("v0.2.0"));
        QVERIFY(notes.contains(QStringLiteral("fixed a bug")));
        QVERIFY(notes.contains(QStringLiteral("<li>")));    // escaped HTML came back unescaped
        QVERIFY(!notes.contains(QStringLiteral("beta")));   // not the first (pre-release) entry
    }
    void picksPreReleaseWhenAskedForIt() {
        const QString notes = notesFromAtom(sampleAtom(), QStringLiteral("v0.3.0-beta"));
        QVERIFY(notes.contains(QStringLiteral("beta preview")));
    }
    void emptyWhenNoMatchOrNoInput() {
        QVERIFY(notesFromAtom(sampleAtom(), QStringLiteral("v9.9.9")).isEmpty());
        QVERIFY(notesFromAtom(QByteArray(), QStringLiteral("v0.2.0")).isEmpty());
    }
    void tagIsNotSuffixMatched() {
        // Asking for "v0.2.0" must not match an entry whose tag merely ends in it.
        const QByteArray atom =
            "<feed xmlns=\"http://www.w3.org/2005/Atom\">"
            "<entry><link href=\"https://github.com/Hunlongyu/hopy/releases/tag/pre-v0.2.0\"/>"
            "<content type=\"html\">nope</content></entry></feed>";
        QVERIFY(notesFromAtom(atom, QStringLiteral("v0.2.0")).isEmpty());
    }
};
QTEST_APPLESS_MAIN(TestUpdateNotes)
#include "test_update_notes.moc"
