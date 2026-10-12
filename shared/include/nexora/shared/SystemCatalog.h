#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace nexora::shared {

// One game system (console, handheld, arcade, ...) that games and emulators can belong to.
struct GameSystem
{
    QString id; // ^[a-z0-9][a-z0-9_-]{0,31}$
    QString name;
    QString shortName;
    QStringList extensions; // lower-case, ".sfc"

    bool operator==(const GameSystem &) const = default;
};

// The list of known systems and the file extensions that identify them.
//
// File format: {"version": 1, "systems": [{"id", "name", "shortName", "extensions": [".ext"]}]}
// Invalid or duplicate entries are dropped with an issue. The catalog shipped with Nexora is
// compiled in; some extensions (.iso, .chd, .zip) intentionally belong to several systems.
class SystemCatalog
{
public:
    static constexpr int FormatVersion = 1;
    static constexpr qint64 MaxFileSize = 256 * 1024;

    // Returns std::nullopt (with a message in |issues|) if the file is unusable as a whole.
    static std::optional<SystemCatalog> load(const QString &file, QStringList *issues);
    // The compiled-in catalog (:/nexora/systems.json).
    static SystemCatalog builtIn();

    static bool isValidId(const QString &id);
    static bool isValidExtension(const QString &ext);

    const QList<GameSystem> &systems() const { return m_systems; }
    const GameSystem *find(const QString &id) const;
    // ".sfc" -> {"snes"}; matching ignores case; ".iso" may map to several systems.
    QStringList systemsForExtension(const QString &ext) const;

private:
    QList<GameSystem> m_systems;
};

} // namespace nexora::shared
