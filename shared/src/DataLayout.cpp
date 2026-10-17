#include "nexora/shared/DataLayout.h"

#include <QDir>
#include <QFile>

namespace nexora::shared {

namespace {

QString xdgBase(const char *variable, const QString &homeRelativeFallback)
{
    const QString value = qEnvironmentVariable(variable);
    if (!value.isEmpty() && QDir::isAbsolutePath(value))
        return QDir::cleanPath(value);
    return QDir::cleanPath(QDir::homePath() + u'/' + homeRelativeFallback);
}

DataLayout build(const QString &config, const QString &data, const QString &cache, const QString &state)
{
    DataLayout l;
    l.configDir = config;
    l.dataDir = data;
    l.savesDir = data + QStringLiteral("/saves");
    l.statesDir = data + QStringLiteral("/states");
    l.emulatorConfigDir = data + QStringLiteral("/emulators");
    l.cacheDir = cache;
    l.stateDir = state;
    l.logDir = state + QStringLiteral("/logs");
    l.diagnosticsDir = state + QStringLiteral("/diagnostics");
    l.backupDir = data + QStringLiteral("/backups");
    return l;
}

} // namespace

DataLayout DataLayout::fromEnvironment()
{
    const QString app = QStringLiteral("/nexora");
    return build(xdgBase("XDG_CONFIG_HOME", QStringLiteral(".config")) + app,
                 xdgBase("XDG_DATA_HOME", QStringLiteral(".local/share")) + app,
                 xdgBase("XDG_CACHE_HOME", QStringLiteral(".cache")) + app,
                 xdgBase("XDG_STATE_HOME", QStringLiteral(".local/state")) + app);
}

DataLayout DataLayout::underRoot(const QString &root)
{
    const QString base = QDir::cleanPath(QDir(root).absolutePath());
    return build(base + QStringLiteral("/config"), base + QStringLiteral("/data"), base + QStringLiteral("/cache"),
                 base + QStringLiteral("/state"));
}

bool DataLayout::ensureDirectories(QString *error) const
{
    // Parents first, so every directory we own is created and then restricted.
    const QStringList dirs{configDir, dataDir,  savesDir, statesDir, emulatorConfigDir,
                           backupDir, cacheDir, stateDir, logDir,    diagnosticsDir};
    const QFileDevice::Permissions ownerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
    for (const QString &dir : dirs) {
        if (!QDir().mkpath(dir)) {
            if (error)
                *error = QStringLiteral("cannot create directory %1").arg(dir);
            return false;
        }
        if (!QFile::setPermissions(dir, ownerOnly)) {
            if (error)
                *error = QStringLiteral("cannot restrict permissions of %1").arg(dir);
            return false;
        }
    }
    return true;
}

} // namespace nexora::shared
