#include "nexora/shared/Redaction.h"

#include <QTest>

using namespace nexora::shared;

class TestRedaction : public QObject
{
    Q_OBJECT

private:
    static RedactionRules rules() { return {QStringLiteral("/home/alice"), {}}; }

private slots:
    void homeDirBecomesTilde()
    {
        QCOMPARE(redact(QStringLiteral("/home/alice/roms/x.sfc"), rules()), QStringLiteral("~/roms/x.sfc"));
        QCOMPARE(redact(QStringLiteral("open /home/alice and /home/alice/a"), rules()),
                 QStringLiteral("open ~ and ~/a"));
    }

    void homeDirOnlyMatchesWholeComponent()
    {
        QCOMPARE(redact(QStringLiteral("/home/alice2/x"), rules()), QStringLiteral("/home/alice2/x"));
    }

    void extraPrefixesBecomeTilde()
    {
        RedactionRules r = rules();
        r.extraPrefixes = {QStringLiteral("/mnt/games")};
        QCOMPARE(redact(QStringLiteral("/mnt/games/a.iso"), r), QStringLiteral("~/a.iso"));
    }

    void urlCredentialsAreStripped()
    {
        QCOMPARE(redact(QStringLiteral("GET https://u:p@host/x"), rules()), QStringLiteral("GET https://host/x"));
        QCOMPARE(redact(QStringLiteral("https://user@host/x"), rules()), QStringLiteral("https://host/x"));
    }

    void keyValueSecretsAreMasked()
    {
        QCOMPARE(redact(QStringLiteral("token=abc"), rules()), QStringLiteral("token=***"));
        QCOMPARE(redact(QStringLiteral("api_key: abc123 next"), rules()), QStringLiteral("api_key: *** next"));
        QCOMPARE(redact(QStringLiteral("a=1 Password=hunter2 b=2"), rules()), QStringLiteral("a=1 Password=*** b=2"));
        QCOMPARE(redact(QStringLiteral("client_secret=zzz"), rules()), QStringLiteral("client_secret=***"));
    }

    void jsonSecretsAreMasked()
    {
        QCOMPARE(redact(QStringLiteral("{\"password\": \"x\", \"name\": \"n\"}"), rules()),
                 QStringLiteral("{\"password\": \"***\", \"name\": \"n\"}"));
        QCOMPARE(redact(QStringLiteral("{\"authToken\":\"abc def\"}"), rules()),
                 QStringLiteral("{\"authToken\":\"***\"}"));
    }

    void unrelatedTextIsUnchanged()
    {
        const QString text = QStringLiteral("Launching snes9x with --fullscreen; monkey=banana turkey: 3");
        QCOMPARE(redact(text, rules()), text);
    }

    void defaultRulesUseHome() { QVERIFY(!defaultRedactionRules().homeDir.isEmpty()); }
};

QTEST_GUILESS_MAIN(TestRedaction)
#include "tst_redaction.moc"
