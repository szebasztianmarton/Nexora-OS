#include "nexora/emulator/EmulatorHealth.h"

#include <QDir>
#include <QFileInfo>

namespace nexora::emulator {

HealthReport checkHealth(const shared::EmulatorConfig &config)
{
    HealthReport report;
    report.checkedAt = QDateTime::currentDateTimeUtc();
    auto set = [&](Health health, const QString &detail) {
        report.health = health;
        report.detail = detail;
        return report;
    };

    if (config.id.isEmpty())
        return set(Health::InvalidConfig, QStringLiteral("missing id"));
    if (!config.enabled)
        return set(Health::Disabled, QStringLiteral("disabled in its configuration"));
    if (config.executable.isEmpty())
        return set(Health::InvalidConfig, QStringLiteral("no executable configured"));
    if (QDir::isRelativePath(config.executable))
        return set(Health::InvalidConfig, QStringLiteral("path must be absolute: %1").arg(config.executable));

    const QFileInfo info(config.executable);
    if (!info.exists())
        return set(Health::ExecutableMissing, config.executable);
    if (!info.isFile())
        return set(Health::NotExecutable, QStringLiteral("not a regular file: %1").arg(config.executable));
    if (!info.isExecutable())
        return set(Health::NotExecutable, QStringLiteral("not executable: %1").arg(config.executable));
    return set(Health::Available, config.executable);
}

QString nameOf(Health health)
{
    switch (health) {
    case Health::Available: return QStringLiteral("Available");
    case Health::Disabled: return QStringLiteral("Disabled");
    case Health::ExecutableMissing: return QStringLiteral("ExecutableMissing");
    case Health::NotExecutable: return QStringLiteral("NotExecutable");
    case Health::InvalidConfig: return QStringLiteral("InvalidConfig");
    }
    return QStringLiteral("Unknown");
}

} // namespace nexora::emulator
