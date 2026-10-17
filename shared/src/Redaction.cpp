#include "nexora/shared/Redaction.h"

#include <QDir>
#include <QRegularExpression>

namespace nexora::shared {

namespace {

bool isSensitiveKey(const QString &name)
{
    const QString lower = name.toLower();
    // Anywhere in the name: authToken, client_secret, aws_credentials, apikey, set-cookie.
    for (const char *needle :
         {"password", "passwd", "passphrase", "token", "secret", "credential", "apikey", "cookie"}) {
        if (lower.contains(QLatin1String(needle)))
            return true;
    }
    // Only as a whole word (split at non-letters and camelCase humps), so that api_key,
    // apiKey, APIKey, session_id and --auth match but monkey, keyboard, author, bypass,
    // compass and sessions do not.
    static const QRegularExpression hump(QStringLiteral("([a-z])([A-Z])|([A-Z])([A-Z][a-z])"));
    static const QRegularExpression separators(QStringLiteral("[^A-Za-z]+"));
    QString spaced = name;
    spaced.replace(hump, QStringLiteral("\\1\\3 \\2\\4"));
    static const QStringList words{QStringLiteral("key"),          QStringLiteral("keys"), QStringLiteral("pass"),
                                   QStringLiteral("pwd"),          QStringLiteral("auth"), QStringLiteral("session"),
                                   QStringLiteral("sessionid"),    QStringLiteral("sid"),  QStringLiteral("bearer"),
                                   QStringLiteral("authorization")};
    for (const QString &word : spaced.split(separators, Qt::SkipEmptyParts)) {
        if (words.contains(word.toLower()))
            return true;
    }
    return false;
}

// "Authorization: <anything>" and "Cookie: <anything>" header lines (also Proxy-
// Authorization and Set-Cookie): the whole value up to the end of the line is secret, not
// just its first word ("Bearer").
QString maskHeaders(const QString &text)
{
    static const QRegularExpression header(
        QStringLiteral(R"((?<![A-Za-z0-9_-])((?:proxy-)?authorization|(?:set-)?cookie)(\s*:[ \t]*)([^\r\n]+))"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = text;
    out.replace(header, QStringLiteral("\\1\\2***"));
    // A bearer token anywhere else (e.g. inside a quoted curl -H argument).
    static const QRegularExpression bearer(QStringLiteral(R"(\b(Bearer\s+)(?!\*\*\*)[A-Za-z0-9._~+/=-]+)"),
                                           QRegularExpression::CaseInsensitiveOption);
    out.replace(bearer, QStringLiteral("\\1***"));
    return out;
}

// "--password hunter2" / "--api-key 'k e y'": a secret-named long option followed by a
// separate value. (The "--password=hunter2" form is handled by maskSecrets() like any
// other key=value pair.) A following option ("--verbose") is not a value.
QString maskFlagValues(const QString &text)
{
    static const QRegularExpression flag(
        QStringLiteral(R"((?<!\S)--([A-Za-z0-9][A-Za-z0-9_.-]*)([ \t]+)("(?:[^"\\]|\\.)*"|'[^']*'|[^\s"'-][^\s"']*))"));
    QString out;
    qsizetype last = 0;
    auto it = flag.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (!isSensitiveKey(m.captured(1)))
            continue;
        const QString value = m.captured(3);
        const QChar quote = value.front();
        const bool quoted = quote == u'"' || quote == u'\'';
        out += QStringView(text).mid(last, m.capturedStart(3) - last);
        out += quoted ? QString(quote) + QStringLiteral("***") + QString(quote) : QStringLiteral("***");
        last = m.capturedEnd(3);
    }
    out += QStringView(text).mid(last);
    return out;
}

QString maskSecrets(const QString &text)
{
    // [quote] name [quote] separator value, where value is "quoted", 'quoted' or a bare word.
    static const QRegularExpression pair(
        QStringLiteral(R"((["']?)([A-Za-z0-9_.-]+)(["']?)(\s*[=:]\s*)("(?:[^"\\]|\\.)*"|'[^']*'|[^\s,;&"'}\]]+))"));
    QString out;
    qsizetype last = 0;
    auto it = pair.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (!isSensitiveKey(m.captured(2)))
            continue;
        const QString value = m.captured(5);
        const QChar quote = value.front();
        const bool quoted = quote == u'"' || quote == u'\'';
        out += QStringView(text).mid(last, m.capturedStart(5) - last);
        out += quoted ? QString(quote) + QStringLiteral("***") + QString(quote) : QStringLiteral("***");
        last = m.capturedEnd(5);
    }
    out += QStringView(text).mid(last);
    return out;
}

QString replacePrefix(QString text, const QString &prefix)
{
    QString cleaned = prefix;
    while (cleaned.size() > 1 && cleaned.endsWith(u'/'))
        cleaned.chop(1);
    if (cleaned.isEmpty() || cleaned == QStringLiteral("/"))
        return text;
    // Whole path components only: "/home/alice" must not match "/home/alice2".
    const QRegularExpression re(QStringLiteral("(?<![A-Za-z0-9_.-])") + QRegularExpression::escape(cleaned)
                                + QStringLiteral("(?![A-Za-z0-9_.-])"));
    return text.replace(re, QStringLiteral("~"));
}

} // namespace

QString redact(const QString &text, const RedactionRules &rules)
{
    QString result = text;

    static const QRegularExpression urlCredentials(QStringLiteral(R"(\b([A-Za-z][A-Za-z0-9+.-]*://)[^/\s@]+@)"));
    result.replace(urlCredentials, QStringLiteral("\\1"));

    result = maskHeaders(result);
    result = maskFlagValues(result);
    result = maskSecrets(result);

    result = replacePrefix(result, rules.homeDir);
    for (const QString &prefix : rules.extraPrefixes)
        result = replacePrefix(result, prefix);
    return result;
}

RedactionRules defaultRedactionRules()
{
    return {QDir::homePath(), {}};
}

} // namespace nexora::shared
