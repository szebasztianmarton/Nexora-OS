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

    void moreSecretKeyNamesAreMasked_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("apikey") << "apikey=abc123" << "apikey=***";
        QTest::newRow("APIKEY") << "APIKEY: abc123" << "APIKEY: ***";
        QTest::newRow("APIKey") << "APIKey=abc" << "APIKey=***";
        QTest::newRow("passphrase") << "passphrase=open sesame" << "passphrase=*** sesame";
        QTest::newRow("pass") << "user=bob pass=hunter2" << "user=bob pass=***";
        QTest::newRow("auth") << "auth=xyz" << "auth=***";
        QTest::newRow("credential") << "aws_credentials: AKIA1" << "aws_credentials: ***";
        QTest::newRow("session") << "session=deadbeef" << "session=***";
        QTest::newRow("session_id json") << R"({"session_id":"s1"})" << R"({"session_id":"***"})";
        QTest::newRow("cookie") << "cookie=abc" << "cookie=***";
        QTest::newRow("bearer key") << "bearer=abc" << "bearer=***";
    }

    void moreSecretKeyNamesAreMasked()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(redact(input, rules()), expected);
    }

    void secretCommandLineFlagsAreMasked_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("--password value") << "retroarch --password hunter2 -f" << "retroarch --password *** -f";
        QTest::newRow("--password=value") << "retroarch --password=hunter2 -f" << "retroarch --password=*** -f";
        QTest::newRow("--api-key value") << "tool --api-key 'k e y' run" << "tool --api-key '***' run";
        QTest::newRow("--token quoted") << R"(tool --token "a b" x)" << R"(tool --token "***" x)";
        QTest::newRow("--auth-token") << "tool --auth-token abc" << "tool --auth-token ***";
        QTest::newRow("next flag is not a value") << "tool --password --verbose" << "tool --password --verbose";
        QTest::newRow("harmless flag") << "tool --fullscreen game.sfc" << "tool --fullscreen game.sfc";
    }

    void secretCommandLineFlagsAreMasked()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(redact(input, rules()), expected);
    }

    void authorizationAndCookieHeadersAreMasked_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::newRow("bearer") << "Authorization: Bearer eyJhbGci.payload.sig" << "Authorization: ***";
        QTest::newRow("basic") << "authorization: Basic dXNlcjpwYXNz\nnext line" << "authorization: ***\nnext line";
        QTest::newRow("proxy") << "Proxy-Authorization: Digest x=1, y=2" << "Proxy-Authorization: ***";
        QTest::newRow("cookie") << "Cookie: session=abc; theme=dark" << "Cookie: ***";
        QTest::newRow("set-cookie") << "Set-Cookie: sid=abc; Path=/; HttpOnly" << "Set-Cookie: ***";
        QTest::newRow("bare bearer") << "curl -H 'X: Bearer abc.def' url" << "curl -H 'X: Bearer ***' url";
    }

    void authorizationAndCookieHeadersAreMasked()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(redact(input, rules()), expected);
    }

    void unrelatedTextIsUnchanged()
    {
        const QString text = QStringLiteral("Launching snes9x with --fullscreen; monkey=banana turkey: 3");
        QCOMPARE(redact(text, rules()), text);
        // Words that merely contain a secret-ish fragment are not secrets.
        const QString words
            = QStringLiteral("author=Ann bypass=1 compass: north passthrough=on sessions=3 keyboard=us");
        QCOMPARE(redact(words, rules()), words);
    }

    void defaultRulesUseHome() { QVERIFY(!defaultRedactionRules().homeDir.isEmpty()); }
};

QTEST_GUILESS_MAIN(TestRedaction)
#include "tst_redaction.moc"
