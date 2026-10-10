#pragma once

#include <QObject>
#include <QStringList>

#include "jsonstore.h"

namespace nexora {

// User preferences persisted to <configDir>/settings.json. Every setter saves
// immediately. Invalid individual values fall back to their defaults without
// discarding the other, valid values.
class AppSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool firstRunCompleted READ firstRunCompleted WRITE setFirstRunCompleted NOTIFY changed)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY changed)          // "en" only for now
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY changed)                    // "dark" only for now
    Q_PROPERTY(qreal interfaceScale READ interfaceScale WRITE setInterfaceScale NOTIFY changed) // 0.8 .. 1.3, default 1.0
    Q_PROPERTY(QString defaultSort READ defaultSort WRITE setDefaultSort NOTIFY changed)  // GameFilterModel sort keys
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY changed)
    Q_PROPERTY(bool sidebarCompact READ sidebarCompact WRITE setSidebarCompact NOTIFY changed)
    Q_PROPERTY(QStringList libraryDirectories READ libraryDirectories NOTIFY changed)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)
    Q_PROPERTY(QString loadError READ loadError NOTIFY changed)   // non-empty if settings.json was unreadable

public:
    explicit AppSettings(const QString &configDirectory, QObject *parent = nullptr);

    bool firstRunCompleted() const;
    void setFirstRunCompleted(bool value);
    QString language() const;
    void setLanguage(const QString &value);
    QString theme() const;
    void setTheme(const QString &value);
    qreal interfaceScale() const;
    void setInterfaceScale(qreal value);
    QString defaultSort() const;
    void setDefaultSort(const QString &value);
    bool reduceMotion() const;
    void setReduceMotion(bool value);
    bool fullscreen() const;
    void setFullscreen(bool value);
    bool sidebarCompact() const;
    void setSidebarCompact(bool value);
    QStringList libraryDirectories() const;
    QString filePath() const;
    QString loadError() const;

    // Returns an empty string on success or a human-readable error
    // ("Directory does not exist", "Already configured", ...). The path is
    // normalised to an absolute, clean path. Never scans anything itself.
    Q_INVOKABLE QString addLibraryDirectory(const QString &path);
    // Only removes the configuration entry; never touches files on disk.
    Q_INVOKABLE bool removeLibraryDirectory(const QString &path);
    // Restores all preferences to defaults. Keeps firstRunCompleted and
    // libraryDirectories unless `includeLibrary` is true.
    Q_INVOKABLE void restoreDefaults(bool includeLibrary = false);

    static QStringList validSortKeys();

signals:
    void changed();
    void libraryDirectoriesChanged();

private:
    void load();
    void save();

    JsonStore m_store;
    QString m_loadError;
    bool m_firstRunCompleted = false;
    QString m_language = QStringLiteral("en");
    QString m_theme = QStringLiteral("dark");
    qreal m_interfaceScale = 1.0;
    QString m_defaultSort = QStringLiteral("title-asc");
    bool m_reduceMotion = false;
    bool m_fullscreen = true;
    bool m_sidebarCompact = false;
    QStringList m_libraryDirectories;
};

} // namespace nexora
