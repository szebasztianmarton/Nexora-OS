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
//  - values of keys named *password*, *passwd*, *passphrase*, *token*, *secret*,
//    *credential*, *apikey*, *cookie*, or containing the word key, pass, pwd, auth,
//    session, sid or bearer (as a word, so "api_key", "apiKey" and "session_id" but not
//    "monkey", "author" or "bypass") become "***" in "k=v", "k: v", JSON "k": "v",
//    "--flag=v" and "--flag v" forms,
//  - Authorization / Proxy-Authorization / Cookie / Set-Cookie header values (to the end of
//    the line) and "Bearer <token>" become "***".
QString redact(const QString &text, const RedactionRules &rules);

RedactionRules defaultRedactionRules(); // homeDir = QDir::homePath()

} // namespace nexora::shared
