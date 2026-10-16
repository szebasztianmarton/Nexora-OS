#include "SystemListModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

using nexora::ui::SystemListModel;

class TestSystemListModel : public QObject
{
    Q_OBJECT

private slots:
    void countsPerSystem()
    {
        SystemListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy countSpy(&model, &SystemListModel::countChanged);

        model.setEntries({
            {QStringLiteral("snes"), QStringLiteral("Super Nintendo"), QStringLiteral("SNES"), 3, 1},
            {QStringLiteral("genesis"), QStringLiteral("Sega Genesis"), QStringLiteral("Genesis"), 1, 1},
        });
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(countSpy.size(), 1);
        const QModelIndex snes = model.index(0);
        QCOMPARE(snes.data(SystemListModel::SystemIdRole).toString(), QStringLiteral("snes"));
        QCOMPARE(snes.data(SystemListModel::NameRole).toString(), QStringLiteral("Super Nintendo"));
        QCOMPARE(snes.data(SystemListModel::ShortNameRole).toString(), QStringLiteral("SNES"));
        QCOMPARE(snes.data(SystemListModel::GameCountRole).toInt(), 3);
        QCOMPARE(snes.data(SystemListModel::EmulatorCountRole).toInt(), 1);
        QVERIFY(model.roleNames().values().contains("systemId"));
    }

    void otherBucketOnlyWhenNonEmpty()
    {
        SystemListModel model;
        // The caller decides whether to include the "" (Other) bucket; the model itself
        // just renders whatever it is given, including an empty-id entry.
        model.setEntries({{QString(), QStringLiteral("Other"), QStringLiteral("Other"), 2, 0}});
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.index(0).data(SystemListModel::SystemIdRole).toString(), QString());

        model.setEntries({});
        QCOMPARE(model.rowCount(), 0);
    }

    void getReturnsRowAsMap()
    {
        SystemListModel model;
        model.setEntries({{QStringLiteral("snes"), QStringLiteral("Super Nintendo"), QStringLiteral("SNES"), 3, 2}});
        const QVariantMap row = model.get(0);
        QCOMPARE(row.value(QStringLiteral("systemId")).toString(), QStringLiteral("snes"));
        QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("Super Nintendo"));
        QCOMPARE(row.value(QStringLiteral("shortName")).toString(), QStringLiteral("SNES"));
        QCOMPARE(row.value(QStringLiteral("gameCount")).toInt(), 3);
        QCOMPARE(row.value(QStringLiteral("emulatorCount")).toInt(), 2);
        QVERIFY(model.get(1).isEmpty());
        QVERIFY(model.get(-1).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSystemListModel)
#include "tst_systemlistmodel.moc"
