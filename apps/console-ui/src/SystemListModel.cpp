#include "SystemListModel.h"

#include <algorithm>

namespace nexora::ui {

SystemListModel::SystemListModel(QObject *parent)
    : QAbstractListModel(parent)
{ }

int SystemListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant SystemListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Entry &e = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return e.name;
    case SystemIdRole: return e.id;
    case ShortNameRole: return e.shortName;
    case GameCountRole: return e.gameCount;
    case EmulatorCountRole: return e.emulatorCount;
    default: return {};
    }
}

QHash<int, QByteArray> SystemListModel::roleNames() const
{
    return {{SystemIdRole, "systemId"},
            {NameRole, "name"},
            {ShortNameRole, "shortName"},
            {GameCountRole, "gameCount"},
            {EmulatorCountRole, "emulatorCount"}};
}

void SystemListModel::setEntries(const QList<Entry> &entries)
{
    // Same systems in the same order (the common case: counts changed after a scan, a
    // favorite or a settings change): update the rows in place. A reset would make views
    // recreate every delegate and drop keyboard focus from the one being operated.
    const bool sameRows = entries.size() == m_entries.size()
        && std::equal(entries.cbegin(), entries.cend(), m_entries.cbegin(),
                      [](const Entry &a, const Entry &b) { return a.id == b.id; });
    if (sameRows) {
        for (int row = 0; row < entries.size(); ++row) {
            const Entry &next = entries.at(row);
            Entry &current = m_entries[row];
            if (next.name == current.name && next.shortName == current.shortName && next.gameCount == current.gameCount
                && next.emulatorCount == current.emulatorCount)
                continue;
            current = next;
            emit dataChanged(index(row), index(row));
        }
        return;
    }
    const int oldCount = rowCount();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (oldCount != rowCount())
        emit countChanged();
}

QVariantMap SystemListModel::get(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    const Entry &e = m_entries.at(row);
    return {{QStringLiteral("systemId"), e.id},
            {QStringLiteral("name"), e.name},
            {QStringLiteral("shortName"), e.shortName},
            {QStringLiteral("gameCount"), e.gameCount},
            {QStringLiteral("emulatorCount"), e.emulatorCount}};
}

} // namespace nexora::ui
