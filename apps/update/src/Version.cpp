#include "nexora/update/Version.h"

#include <QRegularExpression>
#include <QStringList>

namespace nexora::update {

std::optional<Version> Version::parse(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral(R"(^\d{1,4}\.\d{1,4}\.\d{1,4}$)"));
    if (!pattern.match(text).hasMatch())
        return std::nullopt;
    const QStringList parts = text.split(u'.');
    Version v;
    v.major = parts.at(0).toInt();
    v.minor = parts.at(1).toInt();
    v.patch = parts.at(2).toInt();
    return v;
}

QString Version::toString() const
{
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

} // namespace nexora::update
