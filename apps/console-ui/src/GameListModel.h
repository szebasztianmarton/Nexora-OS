#pragma once

#include "nexora/library/GameEntry.h"

#include <QAbstractListModel>
#include <QHash>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace nexora::ui {

// A filtered/sorted view of library entries for one QML list (all games, recent,
// favorites). The owner (LibraryController) decides what goes in.
class GameListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by LibraryController")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        GameIdRole = Qt::UserRole + 1,
        TitleRole,
        PathRole,
        EmulatorIdRole,
        EmulatorNameRole,
        FavoriteRole,
        PlayCountRole,
        LastPlayedRole, // human readable, empty if never
        AvailableRole, // an enabled, working emulator handles the game
        UnavailableReasonRole,
        SystemIdRole,
        SystemShortNameRole,
        DisplayTitleRole,
        PresentRole, // false: the game file is missing (only listed by the "missing" filter)
    };
    Q_ENUM(Role)

    // emulator id -> (display name, unavailable reason or empty)
    using EmulatorInfo = std::function<std::pair<QString, QString>(const QString &emulatorId)>;
    // system id -> short name (empty if unknown or id is empty)
    using SystemInfo = std::function<QString(const QString &systemId)>;

    explicit GameListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEmulatorInfo(EmulatorInfo info) { m_info = std::move(info); }
    void setSystemInfo(SystemInfo info) { m_systemInfo = std::move(info); }
    // system id -> default emulator id (Settings › Emulators). The emulator roles resolve the
    // same emulator as the launch path (LibraryQuery::effectiveEmulatorId) and refresh when
    // the defaults change.
    void setSystemDefaultEmulators(const QHash<QString, QString> &defaults);
    // Replaces the content; keeps rows whose ids did not change in place (no full reset
    // when only flags changed) so QML views keep their current item and focus.
    void setEntries(const QList<library::GameEntry> &entries);
    const QList<library::GameEntry> &entries() const { return m_entries; }

    Q_INVOKABLE int indexOfGame(qint64 gameId) const;
    Q_INVOKABLE qint64 gameIdAt(int row) const;

signals:
    void countChanged();

private:
    QList<library::GameEntry> m_entries;
    EmulatorInfo m_info;
    SystemInfo m_systemInfo;
    QHash<QString, QString> m_systemDefaultEmulators;
};

} // namespace nexora::ui
