#pragma once

#include <QString>
#include <QStringList>

namespace nexora::shared {

// Where Nexora keeps its files, following the XDG base directory specification.
//
//   configDir          settings.json, emulator overrides      $XDG_CONFIG_HOME/nexora
//   dataDir            library.sqlite                         $XDG_DATA_HOME/nexora
//   savesDir           emulator save files (never cleaned)    dataDir/saves
//   statesDir          Quick Resume save states               dataDir/states
//   emulatorConfigDir  per-emulator configuration             dataDir/emulators
//   cacheDir           artwork cache (safe to delete)         $XDG_CACHE_HOME/nexora
//   stateDir           launch history, update state           $XDG_STATE_HOME/nexora
//   logDir             log files                              stateDir/logs
//   diagnosticsDir     diagnostic bundles                     stateDir/diagnostics
//   backupDir          settings / library backups             dataDir/backups
//
// XDG variables that are unset, empty or relative are ignored (per the spec) and the usual
// fallbacks under the home directory are used, so every path is absolute.
struct DataLayout
{
    QString configDir;
    QString dataDir;
    QString savesDir;
    QString statesDir; // Quick Resume save states (docs/quick-resume.md)
    QString emulatorConfigDir;
    QString cacheDir;
    QString stateDir;
    QString logDir;
    QString diagnosticsDir;
    QString backupDir;

    static DataLayout fromEnvironment();
    // For tests: root/config, root/data, root/cache, root/state (+ the same sub-directories).
    static DataLayout underRoot(const QString &root);

    // Creates every directory and restricts it to the owner (0700). Returns false with a
    // message in |error| on the first failure.
    bool ensureDirectories(QString *error) const;

    // The only directories a "clear cache" action may delete.
    QStringList cacheOnlyDirs() const { return {cacheDir}; }
};

} // namespace nexora::shared
