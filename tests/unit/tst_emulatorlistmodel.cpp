#include "EmulatorListModel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

using nexora::shared::EmulatorConfig;
using nexora::ui::EmulatorListModel;

class TestEmulatorListModel : public QObject
{
    Q_OBJECT

private:
    static QList<EmulatorConfig> sample()
    {
        return {EmulatorConfig::make(QStringLiteral("a"), QStringLiteral("Alpha"), QStringLiteral("/bin/true")),
                EmulatorConfig::make(QStringLiteral("b"), QStringLiteral("Beta"), QStringLiteral("/missing"))};
    }

private slots:
    void exposesRoles()
    {
        EmulatorListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy countSpy(&model, &EmulatorListModel::countChanged);
        model.setEmulators(sample(), {{QStringLiteral("b"), QStringLiteral("Emulator executable not found")}});

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(countSpy.size(), 1);
        const QModelIndex a = model.index(0);
        const QModelIndex b = model.index(1);
        QCOMPARE(a.data(EmulatorListModel::EmulatorIdRole).toString(), QStringLiteral("a"));
        QCOMPARE(a.data(EmulatorListModel::NameRole).toString(), QStringLiteral("Alpha"));
        QCOMPARE(a.data(EmulatorListModel::StatusRole).toString(), QStringLiteral("ready"));
        QCOMPARE(a.data(EmulatorListModel::AvailableRole).toBool(), true);
        QCOMPARE(b.data(EmulatorListModel::StatusRole).toString(), QStringLiteral("unavailable"));
        QCOMPARE(b.data(EmulatorListModel::AvailableRole).toBool(), false);
        QCOMPARE(b.data(EmulatorListModel::DetailRole).toString(),
                 QStringLiteral("Emulator executable not found"));
        QVERIFY(model.roleNames().values().contains("emulatorId"));
    }

    void updatesStatus()
    {
        EmulatorListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setEmulators(sample());
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        QVERIFY(model.setStatus(QStringLiteral("b"), QStringLiteral("running"), QStringLiteral("Running")));
        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.first().at(0).toModelIndex().row(), 1);
        QCOMPARE(model.index(1).data(EmulatorListModel::StatusRole).toString(), QStringLiteral("running"));
        QVERIFY(!model.setStatus(QStringLiteral("zzz"), QStringLiteral("running"), QString()));

        model.resetStatuses();
        QCOMPARE(model.index(1).data(EmulatorListModel::StatusRole).toString(), QStringLiteral("ready"));
    }

    void findsEntries()
    {
        EmulatorListModel model;
        model.setEmulators(sample());
        QCOMPARE(model.indexOf(QStringLiteral("b")), 1);
        QCOMPARE(model.indexOf(QStringLiteral("x")), -1);
        QVERIFY(model.find(QStringLiteral("a")).has_value());
        QVERIFY(!model.find(QStringLiteral("x")).has_value());
        QCOMPARE(model.emulatorIdAt(0), QStringLiteral("a"));
        QVERIFY(model.emulatorIdAt(5).isEmpty());
        QVERIFY(model.emulatorIdAt(-1).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestEmulatorListModel)
#include "tst_emulatorlistmodel.moc"
