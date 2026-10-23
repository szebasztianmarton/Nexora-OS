#include "NotificationService.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using nexora::ui::NotificationService;
using Category = NotificationService::Category;
using Severity = NotificationService::Severity;
using namespace std::chrono_literals;

class TestNotificationService : public QObject
{
    Q_OBJECT

private slots:
    void firstPostIsShownAndRecorded()
    {
        NotificationService service;
        QSignalSpy current(&service, &NotificationService::currentChanged);
        QVERIFY(service.post(Category::Library, Severity::Info, QStringLiteral("Added /games")) > 0);
        QCOMPARE(current.size(), 1);
        QCOMPARE(service.current().value(QStringLiteral("text")).toString(), QStringLiteral("Added /games"));
        QCOMPARE(service.current().value(QStringLiteral("category")).toString(), QStringLiteral("library"));
        QCOMPARE(service.history().size(), 1);
        QCOMPARE(service.post(Category::Library, Severity::Info, QStringLiteral("   ")), quint64(0));
    }

    void repeatedEventsAreDeduplicated()
    {
        NotificationService service;
        const quint64 id = service.post(Category::Controller, Severity::Warning, QStringLiteral("Pad gone"));
        QCOMPARE(service.post(Category::Controller, Severity::Warning, QStringLiteral("Pad gone")), id);
        QCOMPARE(service.post(Category::Controller, Severity::Warning, QStringLiteral("Pad gone")), id);
        QCOMPARE(service.current().value(QStringLiteral("count")).toInt(), 3);
        QCOMPARE(service.history().size(), 1);
        QCOMPARE(service.queuedCount(), 0);
        // Same text in another category is a different event.
        QVERIFY(service.post(Category::Game, Severity::Warning, QStringLiteral("Pad gone")) != id);
        QCOMPARE(service.history().size(), 2);
    }

    void dedupWindowExpires()
    {
        NotificationService service;
        service.setDedupWindow(50ms);
        const quint64 first = service.post(Category::Game, Severity::Info, QStringLiteral("Started"));
        QTest::qWait(120);
        QVERIFY(service.post(Category::Game, Severity::Info, QStringLiteral("Started")) != first);
    }

    void explicitKeysDeduplicateDifferentWording()
    {
        NotificationService service;
        const quint64 id = service.post(Category::Storage, Severity::Warning, QStringLiteral("Low: 900 MB"),
                                        QStringLiteral("storage-low"));
        QCOMPARE(service.post(Category::Storage, Severity::Warning, QStringLiteral("Low: 850 MB"),
                              QStringLiteral("storage-low")),
                 id);
        QCOMPARE(service.current().value(QStringLiteral("text")).toString(), QStringLiteral("Low: 850 MB"));
    }

    void queueShowsOneAtATimeAndCriticalJumpsAhead()
    {
        NotificationService service;
        service.post(Category::Library, Severity::Info, QStringLiteral("one"));
        service.post(Category::Library, Severity::Info, QStringLiteral("two"));
        QCOMPARE(service.queuedCount(), 1);
        service.post(Category::Recovery, Severity::Critical, QStringLiteral("state lost"));
        // The critical one replaces the current info notification, which goes back in line.
        QCOMPARE(service.current().value(QStringLiteral("text")).toString(), QStringLiteral("state lost"));
        service.dismiss();
        QCOMPARE(service.current().value(QStringLiteral("text")).toString(), QStringLiteral("one"));
        service.dismiss();
        QCOMPARE(service.current().value(QStringLiteral("text")).toString(), QStringLiteral("two"));
        service.dismiss();
        QVERIFY(service.current().isEmpty());
    }

    void notificationsHideAfterTheirDuration()
    {
        NotificationService service;
        service.setDurationMs(500); // the setter's lower bound
        service.post(Category::Library, Severity::Info, QStringLiteral("short"));
        QTRY_VERIFY_WITH_TIMEOUT(service.current().isEmpty(), 3000);
        service.post(Category::Game, Severity::Warning, QStringLiteral("longer"));
        QTest::qWait(700);
        QVERIFY(!service.current().isEmpty()); // warnings stay twice as long
        QTRY_VERIFY_WITH_TIMEOUT(service.current().isEmpty(), 3000);
    }

    void quietModeAndHiddenCategoriesOnlyRecord()
    {
        NotificationService service;
        service.setQuiet(true);
        service.post(Category::Library, Severity::Info, QStringLiteral("info in quiet mode"));
        QVERIFY(service.current().isEmpty());
        QCOMPARE(service.history().size(), 1);
        service.post(Category::Game, Severity::Warning, QStringLiteral("warning in quiet mode"));
        QVERIFY(!service.current().isEmpty()); // quiet mode only silences info
        service.dismiss();

        service.setQuiet(false);
        service.setHiddenCategories({QStringLiteral("controller"), QStringLiteral("bogus")});
        service.post(Category::Controller, Severity::Warning, QStringLiteral("hidden"));
        QVERIFY(service.current().isEmpty());
        service.post(Category::Controller, Severity::Critical, QStringLiteral("critical always shows"));
        QCOMPARE(service.current().value(QStringLiteral("severity")).toString(), QStringLiteral("critical"));
        QCOMPARE(service.history().size(), 4);
    }

    void historyIsBoundedNewestFirst()
    {
        NotificationService service;
        service.setQuiet(true);
        for (int i = 0; i < NotificationService::MaxHistory + 20; ++i)
            service.post(Category::Library, Severity::Info, QStringLiteral("event %1").arg(i));
        QCOMPARE(service.history().size(), NotificationService::MaxHistory);
        QCOMPARE(service.history().first().toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("event %1").arg(NotificationService::MaxHistory + 19));
        service.clearHistory();
        QVERIFY(service.history().isEmpty());
    }

    // Warnings and criticals survive a restart (e.g. "the game could not be saved" reported
    // while shutting down); info notifications do not.
    void importantHistoryIsPersisted()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("notifications.json"));
        {
            NotificationService service(file);
            service.post(Category::Library, Severity::Info, QStringLiteral("not kept"));
            service.post(Category::SaveState, Severity::Warning, QStringLiteral("Save failed"));
            service.post(Category::Recovery, Severity::Critical, QStringLiteral("Recovery needed"));
        }
        NotificationService again(file);
        again.loadHistory();
        const QVariantList history = again.history();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("Recovery needed"));
        QVERIFY(history.at(0).toMap().value(QStringLiteral("fromPreviousRun")).toBool());
        QVERIFY(again.current().isEmpty()); // old events are history, not new popups
        // An event of this run is not merged into the previous run's entry.
        again.post(Category::SaveState, Severity::Warning, QStringLiteral("Save failed"));
        QCOMPARE(again.history().size(), 3);
    }

    // main.cpp posts "Controller connected" before the history is loaded: this run's entries
    // stay in front of the previous run's, and trimming drops the oldest previous-run ones.
    void loadedHistoryGoesBehindThisRunsEntries()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("notifications.json"));
        {
            NotificationService previous(file);
            for (int i = 0; i < NotificationService::MaxPersisted; ++i)
                previous.post(Category::SaveState, Severity::Warning, QStringLiteral("old %1").arg(i));
        }
        NotificationService service(file);
        service.setQuiet(true);
        const int thisRun = NotificationService::MaxHistory - NotificationService::MaxPersisted + 10;
        for (int i = 0; i < thisRun; ++i)
            service.post(Category::Library, Severity::Info, QStringLiteral("new %1").arg(i));
        service.loadHistory();
        const QVariantList history = service.history();
        QCOMPARE(history.size(), NotificationService::MaxHistory);
        QCOMPARE(history.first().toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("new %1").arg(thisRun - 1));
        QVERIFY(!history.first().toMap().value(QStringLiteral("fromPreviousRun")).toBool());
        QCOMPARE(history.at(thisRun - 1).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("new 0"));
        QCOMPARE(history.at(thisRun).toMap().value(QStringLiteral("text")).toString(),
                 QStringLiteral("old %1").arg(NotificationService::MaxPersisted - 1));
        QVERIFY(history.last().toMap().value(QStringLiteral("fromPreviousRun")).toBool());
    }

    void corruptHistoryFileIsMovedAside()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("notifications.json"));
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{not json");
        f.close();
        NotificationService service(file);
        service.loadHistory();
        QVERIFY(service.history().isEmpty());
        QVERIFY(!QFile::exists(file));
    }

    void qmlProducersMapCategoryAndSeverity()
    {
        NotificationService service;
        service.postMessage(QStringLiteral("library"), QStringLiteral("Scan failed"), true);
        QCOMPARE(service.current().value(QStringLiteral("severity")).toString(), QStringLiteral("warning"));
        QCOMPARE(service.current().value(QStringLiteral("category")).toString(), QStringLiteral("library"));
        service.dismiss();
        service.postMessage(QStringLiteral("unknown"), QStringLiteral("Hello"), false);
        QCOMPARE(service.current().value(QStringLiteral("category")).toString(), QStringLiteral("system"));
    }
};

QTEST_GUILESS_MAIN(TestNotificationService)
#include "tst_notificationservice.moc"
