#include "GameListModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

using nexora::library::GameEntry;
using nexora::ui::GameListModel;

namespace {

GameEntry game(qint64 id, const QString &title, const QStringList &candidates, const QString &systemId = {})
{
    GameEntry e;
    e.id = id;
    e.title = title;
    e.path = QStringLiteral("/games/%1.sfc").arg(title);
    e.extension = QStringLiteral(".sfc");
    e.emulatorCandidates = candidates;
    e.systemId = systemId;
    return e;
}

// Emulator "a" is broken, "b" works; both have display names.
std::pair<QString, QString> emulatorInfo(const QString &emulatorId)
{
    if (emulatorId == QStringLiteral("a"))
        return {QStringLiteral("Emu A"), QStringLiteral("Emu A is not installed")};
    if (emulatorId == QStringLiteral("b"))
        return {QStringLiteral("Emu B"), QString()};
    return {};
}

} // namespace

class TestGameListModel : public QObject
{
    Q_OBJECT

private slots:
    // The tile must name (and grey out for) the emulator that would actually launch:
    // the per-game override, else the system default, else the first candidate.
    void rolesFollowTheSystemDefault()
    {
        GameListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setEmulatorInfo(emulatorInfo);
        model.setEntries(
            {game(1, QStringLiteral("Mario"), {QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("snes"))});
        const QModelIndex row = model.index(0);
        QCOMPARE(row.data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
        QCOMPARE(row.data(GameListModel::EmulatorNameRole).toString(), QStringLiteral("Emu A"));
        QVERIFY(!row.data(GameListModel::AvailableRole).toBool());
        QCOMPARE(row.data(GameListModel::UnavailableReasonRole).toString(), QStringLiteral("Emu A is not installed"));

        QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
        model.setSystemDefaultEmulators({{QStringLiteral("snes"), QStringLiteral("b")}});
        QCOMPARE(changes.size(), 1);
        QCOMPARE(changes.at(0).at(0).toModelIndex().row(), 0);
        QCOMPARE(changes.at(0).at(1).toModelIndex().row(), 0);
        const QList<int> roles = changes.at(0).at(2).value<QList<int>>();
        QVERIFY(roles.contains(GameListModel::EmulatorIdRole));
        QVERIFY(roles.contains(GameListModel::EmulatorNameRole));
        QVERIFY(roles.contains(GameListModel::AvailableRole));
        QVERIFY(roles.contains(GameListModel::UnavailableReasonRole));
        QCOMPARE(row.data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("b"));
        QCOMPARE(row.data(GameListModel::EmulatorNameRole).toString(), QStringLiteral("Emu B"));
        QVERIFY(row.data(GameListModel::AvailableRole).toBool());
        QCOMPARE(row.data(GameListModel::UnavailableReasonRole).toString(), QString());

        // The same defaults again change nothing; back to automatic refreshes the rows again.
        model.setSystemDefaultEmulators({{QStringLiteral("snes"), QStringLiteral("b")}});
        QCOMPARE(changes.size(), 1);
        model.setSystemDefaultEmulators({});
        QCOMPARE(changes.size(), 2);
        QCOMPARE(row.data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
    }

    void overrideBeatsTheSystemDefault()
    {
        GameListModel model;
        model.setEmulatorInfo(emulatorInfo);
        model.setSystemDefaultEmulators({{QStringLiteral("snes"), QStringLiteral("b")}});
        GameEntry e
            = game(1, QStringLiteral("Mario"), {QStringLiteral("a"), QStringLiteral("b")}, QStringLiteral("snes"));
        e.emulatorOverride = QStringLiteral("a");
        model.setEntries({e});
        QCOMPARE(model.index(0).data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
        // A default that does not handle the file, or a game without a system, is ignored.
        model.setEntries({game(2, QStringLiteral("Only A"), {QStringLiteral("a")}, QStringLiteral("snes")),
                          game(3, QStringLiteral("No system"), {QStringLiteral("a"), QStringLiteral("b")})});
        QCOMPARE(model.index(0).data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
        QCOMPARE(model.index(1).data(GameListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
    }

    void noCandidateIsUnavailable()
    {
        GameListModel model;
        model.setEmulatorInfo(emulatorInfo);
        model.setEntries({game(1, QStringLiteral("Orphan"), {})});
        QVERIFY(!model.index(0).data(GameListModel::AvailableRole).toBool());
        QCOMPARE(model.index(0).data(GameListModel::UnavailableReasonRole).toString(),
                 QStringLiteral("No emulator handles .sfc files"));
        model.setSystemDefaultEmulators({{QStringLiteral("snes"), QStringLiteral("b")}});
        QVERIFY(!model.index(0).data(GameListModel::AvailableRole).toBool());
    }
};

QTEST_GUILESS_MAIN(TestGameListModel)
#include "tst_gamelistmodel.moc"
