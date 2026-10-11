#include "nexora/shared/Redaction.h"

#include <QDir>
#include <QRegularExpression>

namespace nexora::shared {

namespace {

bool isSensitiveKey(const QString &name)
{
    const QString lower = name.toLower();
    for (const char *needle : {"password", "passwd", "token", "secret"}) {
        if (lower.contains(QLatin1String(needle)))
            return true;
    }
    // "key" only as a word: key, api_key, api-key, apiKey, but not monkey or keyboard.
    static const QRegularExpression keyWord(QStringLiteral("(^|[^A-Za-z])[Kk][Ee][Yy]($|[^a-z])|[a-z]Key($|[^a-z])"));
    return keyWord.match(name).hasMatch();
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
