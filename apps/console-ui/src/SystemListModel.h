#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace nexora::ui {

// Game systems known to the library, with how many visible games and enabled emulators
// belong to each. Built by LibraryController from the system catalog, the current game
// entries and the enabled emulator definitions.
class SystemListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by LibraryController")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        SystemIdRole = Qt::UserRole + 1,
        NameRole,
        ShortNameRole,
        GameCountRole,
        EmulatorCountRole,
    };
    Q_ENUM(Role)

    struct Entry
    {
        QString id; // "" = games/emulators with no recognised system ("Other")
        QString name;
        QString shortName;
        int gameCount = 0;
        int emulatorCount = 0;
    };

    explicit SystemListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the content. |entries| should already have its "Other" bucket (id "")
    // dropped by the caller when it is empty.
    void setEntries(const QList<Entry> &entries);

signals:
    void countChanged();

private:
    QList<Entry> m_entries;
};

} // namespace nexora::ui
