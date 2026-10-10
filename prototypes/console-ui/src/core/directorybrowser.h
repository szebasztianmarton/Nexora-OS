#pragma once

#include <QObject>
#include <QStringList>

namespace nexora {

// Controller-friendly folder picker backend. Lists only sub-directories (no
// files), never follows into unreadable folders, and performs no scanning.
class DirectoryBrowser : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QStringList entries READ entries NOTIFY currentPathChanged)  // sub-directory names, sorted, hidden dirs excluded
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY currentPathChanged)
    Q_PROPERTY(QString error READ error NOTIFY currentPathChanged)           // e.g. "Permission denied"
    Q_PROPERTY(QVariantList locations READ locations CONSTANT)              // [{name, path}] existing: Home, /media, /mnt, /run/media/<user>, /

public:
    explicit DirectoryBrowser(QObject *parent = nullptr);

    QString currentPath() const;
    QStringList entries() const;
    bool canGoUp() const;
    QString error() const;
    QVariantList locations() const;

    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE bool enter(const QString &name);
    Q_INVOKABLE bool up();
    Q_INVOKABLE void home();

signals:
    void currentPathChanged();

private:
    QString m_path;
    QStringList m_entries;
    QString m_error;
};

} // namespace nexora
