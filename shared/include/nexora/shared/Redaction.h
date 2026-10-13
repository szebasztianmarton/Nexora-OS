#pragma once

#include <QString>
#include <QStringList>

namespace nexora::shared {

struct RedactionRules
{
    QString homeDir; // replaced by "~"
    QStringList extraPrefixes; // other private path prefixes, also replaced by "~"
};

// Makes text safe to put in logs and diagnostic bundles:
//  - directory prefixes from |rules| become "~" (whole path components only),
//  - "user:pass@" is stripped from URLs,
//  - values of keys named *password*, *passwd*, *token*, *secret* or *key* (as a word, so
//    "api_key" and "apiKey" but not "monkey") become "***" in "k=v", "k: v" and JSON
//    "k": "v" forms.
QString redact(const QString &text, const RedactionRules &rules);

RedactionRules defaultRedactionRules(); // homeDir = QDir::homePath()

} // namespace nexora::shared
