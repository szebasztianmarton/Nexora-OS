#include "nexora/shared/Paths.h"

#include <QDir>

#include <unistd.h>

namespace nexora::shared {

ConfigDirResolution resolveConfigDirectory(const QString &commandLineValue, const QString &environmentValue,
                                           const QString &applicationDir)
{
    if (!commandLineValue.isEmpty())
        return {QDir(commandLineValue).absolutePath(), ConfigDirSource::CommandLine};
    if (!environmentValue.isEmpty())
        return {QDir(environmentValue).absolutePath(), ConfigDirSource::Environment};
    return {QDir::cleanPath(QDir(applicationDir).absoluteFilePath(QStringLiteral("../share/nexora/emulators"))),
            ConfigDirSource::Default};
}

QString toString(ConfigDirSource source)
{
    switch (source) {
    case ConfigDirSource::CommandLine: return QStringLiteral("command line (--config-dir)");
    case ConfigDirSource::Environment: return QStringLiteral("environment (NEXORA_CONFIG_DIR)");
    case ConfigDirSource::Default: return QStringLiteral("default location");
    }
    return {};
}

bool isRunningAsRoot()
{
    return ::geteuid() == 0;
}

} // namespace nexora::shared
