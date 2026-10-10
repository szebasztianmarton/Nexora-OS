#include "EmulatorListModel.h"

namespace nexora::ui {

EmulatorListModel::EmulatorListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int EmulatorListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant EmulatorListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return entry.config.name;
    case EmulatorIdRole: return entry.config.id;
    case ExecutableRole: return entry.config.executable;
    case StatusRole: return entry.status;
    case StatusTextRole: return entry.statusText;
    case DetailRole: return entry.detail;
    case AvailableRole: return entry.available;
    case ProfilesRole: {
        QVariantList profiles;
        for (const auto &p : entry.config.profiles)
            profiles.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name},
                                        {QStringLiteral("enabled"), p.enabled}});
        return profiles;
    }
    case ExtensionsRole: return entry.config.extensions.join(QStringLiteral(", "));
    default: return {};
    }
}

QHash<int, QByteArray> EmulatorListModel::roleNames() const
{
    return {
        {EmulatorIdRole, "emulatorId"},
        {NameRole, "name"},
        {ExecutableRole, "executable"},
        {StatusRole, "status"},
        {StatusTextRole, "statusText"},
        {DetailRole, "detail"},
        {AvailableRole, "available"},
        {ProfilesRole, "profiles"},
        {ExtensionsRole, "extensions"},
    };
}

void EmulatorListModel::applyBaseStatus(Entry &entry)
{
    if (entry.available) {
        entry.status = QStringLiteral("ready");
        entry.statusText = QStringLiteral("Ready");
        entry.detail.clear();
    } else {
        entry.status = QStringLiteral("unavailable");
        entry.statusText = QStringLiteral("Unavailable");
        entry.detail = entry.unavailableReason;
    }
}

void EmulatorListModel::setEmulators(const QList<shared::EmulatorConfig> &emulators,
                                     const QHash<QString, QString> &unavailableReasons)
{
    const int oldCount = rowCount();
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(emulators.size());
    for (const auto &config : emulators) {
        Entry entry;
        entry.config = config;
        entry.unavailableReason = unavailableReasons.value(config.id);
        entry.available = !unavailableReasons.contains(config.id);
        applyBaseStatus(entry);
        m_entries.append(entry);
    }
    endResetModel();
    if (oldCount != rowCount())
        emit countChanged();
}

bool EmulatorListModel::setStatus(const QString &id, const QString &status,
                                  const QString &statusText, const QString &detail)
{
    const int row = indexOf(id);
    if (row < 0)
        return false;
    Entry &entry = m_entries[row];
    entry.status = status;
    entry.statusText = statusText;
    entry.detail = detail;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {StatusRole, StatusTextRole, DetailRole});
    return true;
}

void EmulatorListModel::resetStatuses()
{
    if (m_entries.isEmpty())
        return;
    for (Entry &entry : m_entries)
        applyBaseStatus(entry);
    emit dataChanged(index(0), index(rowCount() - 1), {StatusRole, StatusTextRole, DetailRole});
}

int EmulatorListModel::indexOf(const QString &id) const
{
    for (qsizetype i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).config.id == id)
            return int(i);
    }
    return -1;
}

std::optional<shared::EmulatorConfig> EmulatorListModel::find(const QString &id) const
{
    const int row = indexOf(id);
    if (row < 0)
        return std::nullopt;
    return m_entries.at(row).config;
}

QString EmulatorListModel::emulatorIdAt(int row) const
{
    if (row < 0 || row >= rowCount())
        return {};
    return m_entries.at(row).config.id;
}

} // namespace nexora::ui
