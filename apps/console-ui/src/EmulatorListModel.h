#pragma once

#include "nexora/shared/EmulatorConfig.h"

#include <QAbstractListModel>
#include <QList>
#include <QtQml/qqmlregistration.h>

#include <optional>

namespace nexora::ui {

// Enabled emulator definitions plus their per-entry UI status.
class EmulatorListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        EmulatorIdRole = Qt::UserRole + 1,
        NameRole,
        ExecutableRole,
        StatusRole, // ready, unavailable, starting, running, stopping, exited, failed
        StatusTextRole, // short human-readable status
        DetailRole, // longer explanation (e.g. why an entry is unavailable)
        AvailableRole, // executable passed validation at load time
        ProfilesRole, // [{id, name, enabled}]
        ExtensionsRole, // ".sfc, .smc"
    };
    Q_ENUM(Role)

    explicit EmulatorListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the content. Each entry starts as "ready", or "unavailable" when
    // |unavailableReasons| contains its id.
    void setEmulators(const QList<shared::EmulatorConfig> &emulators,
                      const QHash<QString, QString> &unavailableReasons = {});
    bool setStatus(const QString &id, const QString &status, const QString &statusText, const QString &detail = {});
    // Resets every entry to its load-time status (ready / unavailable).
    void resetStatuses();

    int indexOf(const QString &id) const;
    std::optional<shared::EmulatorConfig> find(const QString &id) const;
    Q_INVOKABLE QString emulatorIdAt(int row) const;

signals:
    void countChanged();

private:
    struct Entry
    {
        shared::EmulatorConfig config;
        bool available = true;
        QString unavailableReason;
        QString status;
        QString statusText;
        QString detail;
    };
    static void applyBaseStatus(Entry &entry);

    QList<Entry> m_entries;
};

} // namespace nexora::ui
