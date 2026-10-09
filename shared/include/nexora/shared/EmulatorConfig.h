#pragma once

#include <QString>
#include <QStringList>

namespace nexora::shared {

// One validated emulator definition, loaded from a single JSON file.
struct EmulatorConfig
{
    QString id;
    QString name;
    QString executable; // absolute, cleaned path; never resolved through PATH
    QStringList arguments;
    bool enabled = true;
    QString sourceFile; // JSON file this entry was loaded from (diagnostics only)

    bool operator==(const EmulatorConfig &) const = default;
};

} // namespace nexora::shared
