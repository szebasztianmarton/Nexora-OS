#include "nexora/update/Version.h"

#include <QTest>

using nexora::update::Version;

class TestVersion : public QObject
{
    Q_OBJECT
private slots:
    void parsesValid()
    {
        const auto v = Version::parse(QStringLiteral("0.4.1"));
        QVERIFY(v.has_value());
        QCOMPARE(v->major, 0);
        QCOMPARE(v->minor, 4);
        QCOMPARE(v->patch, 1);
        QCOMPARE(v->toString(), QStringLiteral("0.4.1"));
    }

    void acceptsLeadingZeros()
    {
        const auto v = Version::parse(QStringLiteral("01.02.03"));
        QVERIFY(v.has_value());
        QCOMPARE(*v, (Version{1, 2, 3}));
    }

    void rejectsMalformed_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("too few components") << QStringLiteral("1.2");
        QTest::newRow("leading v") << QStringLiteral("v1.2.3");
        QTest::newRow("pre-release suffix") << QStringLiteral("1.2.3-rc1");
        QTest::newRow("empty") << QString();
        QTest::newRow("too many components") << QStringLiteral("1.2.3.4");
        QTest::newRow("non-numeric") << QStringLiteral("a.b.c");
        QTest::newRow("trailing dot") << QStringLiteral("1.2.3.");
        QTest::newRow("leading dot") << QStringLiteral(".1.2.3");
        QTest::newRow("negative") << QStringLiteral("-1.2.3");
        QTest::newRow("whitespace") << QStringLiteral(" 1.2.3");
    }

    void rejectsMalformed()
    {
        QFETCH(QString, text);
        QVERIFY(!Version::parse(text).has_value());
    }

    void comparesBySemver()
    {
        QVERIFY((Version{0, 4, 0} < Version{0, 4, 1}));
        QVERIFY((Version{0, 4, 1} < Version{0, 5, 0}));
        QVERIFY((Version{0, 9, 9} < Version{1, 0, 0}));
        QVERIFY((Version{1, 0, 0} == Version{1, 0, 0}));
        QVERIFY(!(Version{1, 0, 0} < Version{1, 0, 0}));
    }
};

QTEST_APPLESS_MAIN(TestVersion)
#include "tst_version.moc"
