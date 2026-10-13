#include "SystemListModel.h"

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
    const int oldCount = rowCount();
    beginResetModel();
    m_entries = entries;
    endResetModel();
    if (oldCount != rowCount())
        emit countChanged();
}

} // namespace nexora::ui
