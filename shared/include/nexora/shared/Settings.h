#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace nexora::shared {

// Layered, validated application preferences.
//
// Effective value of a key = built-in default, overridden by the system defaults file
// (read-only, e.g. shipped with the image), overridden by the user file (written by the
// application). Only user overrides are ever written. Every value is validated on load and
// on setValue(); invalid or unknown entries are dropped with an issue instead of failing.
// A user file that cannot be parsed is moved aside (settings.json.corrupt-<timestamp>) and
// defaults are used.
//
// File format: {"version": 1, "settings": {"<key>": <value>, ...}}
class Settings : public QObject
{
    Q_OBJECT

public:
    static constexpr int FormatVersion = 1;
    static constexpr qint64 MaxFileSize = 256 * 1024;

    // Keys (flat, dotted).
    static inline const QString UiLanguage = QStringLiteral("ui.language");
    static inline const QString UiShowHints = QStringLiteral("ui.showHints");
    static inline const QString LibrarySortOrder = QStringLiteral("library.sortOrder");
    static inline const QString LibraryDirectories = QStringLiteral("library.directories");
    static inline const QString LibraryExtensions = QStringLiteral("library.extensions");
    static inline const QString LibraryFollowSymlinks = QStringLiteral("library.followSymlinks");
    static inline const QString DisplayMode = QStringLiteral("display.mode");
    static inline const QString DisplayResolution = QStringLiteral("display.resolution");
    static inline const QString AudioVolume = QStringLiteral("audio.volume");
    static inline const QString InputBackend = QStringLiteral("input.backend");
    static inline const QString InputMapping = QStringLiteral("input.mapping");
    static inline const QString EmulatorDefaultProfiles = QStringLiteral("emulators.defaultProfiles");

    struct Paths
    {
        QStringList systemDefaultFiles; // applied in order; missing files are fine
        QString userFile;
    };

    struct Spec
    {
        QString key;
        QVariant defaultValue;
        QString description;
        // Returns an error message, or an empty string if |value| is acceptable. May
        // normalise |value| in place (e.g. trim strings).
        std::function<QString(QVariant &value)> validate;
    };

    explicit Settings(Paths paths, QObject *parent = nullptr);

    // (Re)reads all layers. Returns false only if the user file existed but was unusable.
    bool load();
    // Atomically writes the user overrides. Creates the parent directory if needed.
    bool save();
    // Drops all user overrides; the previous user file is kept as <file>.bak.
    bool resetToDefaults();

    QVariant value(const QString &key) const;
    QString stringValue(const QString &key) const { return value(key).toString(); }
    bool boolValue(const QString &key) const { return value(key).toBool(); }
    int intValue(const QString &key) const { return value(key).toInt(); }
    QStringList stringListValue(const QString &key) const { return value(key).toStringList(); }
    QVariantMap mapValue(const QString &key) const { return value(key).toMap(); }

    // Validates and stores |value| as a user override (in memory; call save() to persist).
    // Setting a value equal to the layered default removes the override.
    bool setValue(const QString &key, const QVariant &value, QString *error = nullptr);
    bool isOverridden(const QString &key) const { return m_user.contains(key); }

    QStringList issues() const { return m_issues; }
    const Paths &paths() const { return m_paths; }
    static const QList<Spec> &specs();
    static const Spec *spec(const QString &key);

    // $XDG_CONFIG_HOME/nexora/settings.json (QStandardPaths::GenericConfigLocation).
    static QString defaultUserFile();
    // <appDir>/../share/nexora/settings-defaults.json and /etc/nexora/settings-defaults.json.
    static QStringList defaultSystemFiles(const QString &applicationDir);

signals:
    void valueChanged(const QString &key, const QVariant &value);
    void reloaded();

private:
    enum class Layer { System, User };
    std::optional<QVariantMap> readLayer(const QString &file, Layer layer);
    QVariant layeredDefault(const QString &key) const;

    Paths m_paths;
    QVariantMap m_system; // validated system default overrides
    QVariantMap m_user;   // validated user overrides
    QStringList m_issues;
};

} // namespace nexora::shared
