#pragma once

#include <QString>

namespace nexora::shared {

enum class ConfigDirSource { CommandLine, Environment, Default };

struct ConfigDirResolution
{
    QString path;
    ConfigDirSource source = ConfigDirSource::Default;
};

// Environment variable that overrides the default configuration directory.
inline constexpr const char *ConfigDirEnvVar = "NEXORA_CONFIG_DIR";

// Precedence: command line > environment > <appDir>/../share/nexora/emulators.
ConfigDirResolution resolveConfigDirectory(const QString &commandLineValue, const QString &environmentValue,
                                           const QString &applicationDir);

QString toString(ConfigDirSource source);

bool isRunningAsRoot();

} // namespace nexora::shared
