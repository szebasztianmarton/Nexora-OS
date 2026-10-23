// Quick Resume end to end: GameSessionManager + AppController + the control-socket adapter
// against nexora-fake-emulator, with state files and meta.json on disk.
#include "AppController.h"
#include "GameSessionManager.h"
#include "NotificationService.h"

#include "nexora/shared/Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using nexora::library::GameEntry;
using nexora::shared::Settings;
using nexora::ui::AppController;
using nexora::ui::GameSessionManager;
using nexora::ui::NotificationService;
using State = GameSessionManager::State;

namespace {

constexpr int Timeout = 10000;

void write(const QString &path, const QByteArray &content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool historyContains(const NotificationService &notes, const QString &text)
{
    for (const auto &n : notes.historyEntries()) {
        if (n.text.contains(text))
            return true;
    }
    return false;
}

} // namespace

// One console: configuration, settings, notifications, app and session manager under a
// temporary directory. |extraArguments| are passed to the fake emulator.
struct Console
{
    explicit Console(const QString &root, const QString &extraArguments = {}, bool saveStates = true,
                     const QString &version = QStringLiteral("1.0"))
        : settings({{}, root + QStringLiteral("/settings.json")})
    {
        QDir().mkpath(root + QStringLiteral("/emulators"));
        writeEmulator(root, extraArguments, saveStates, version);
        settings.load();
        AppController::Options options;
        options.configDirectory = root + QStringLiteral("/emulators");
        app = std::make_unique<AppController>(options);
        GameSessionManager::Dependencies deps;
        deps.app = app.get();
        deps.settings = &settings;
        deps.notifications = &notes;
        deps.statesDir = root + QStringLiteral("/states");
        sessions = std::make_unique<GameSessionManager>(deps);
        sessions->load();
    }
    ~Console()
    {
        sessions.reset();
        app->manager()->shutdown();
    }

    static void writeEmulator(const QString &root, const QString &extraArguments, bool saveStates,
                              const QString &version, const QString &id = QStringLiteral("fake"))
    {
        const QString caps = saveStates
            ? QStringLiteral(",\"capabilities\":{\"pause\":true,\"saveState\":{\"adapter\":\"control-socket\","
                             "\"timeoutMs\":3000,\"launchArguments\":[\"--control-socket\",\"{control-socket}\"],"
                             "\"resumeArguments\":[\"--load-state\",\"{state-file}\"]}}")
            : QString();
        const QString args = QStringLiteral("\"--sleep-ms\",\"30000\"")
            + (extraArguments.isEmpty() ? QString() : QStringLiteral(",") + extraArguments);
        const QString name = id == QLatin1String("fake") ? QStringLiteral("Fake") : QStringLiteral("Fake ") + id;
        write(root + QStringLiteral("/emulators/") + id + QStringLiteral(".json"),
              QStringLiteral("{\"id\":\"%5\",\"name\":\"%6\",\"version\":\"%1\",\"executable\":\"%2\","
                             "\"arguments\":[%3],\"extensions\":[\".sfc\"]%4}")
                  .arg(version, QStringLiteral(NEXORA_FAKE_EMULATOR), args, caps, id, name)
                  .toUtf8());
    }

    GameEntry game(const QString &root, qint64 id, const QString &name)
    {
        GameEntry e;
        e.id = id;
        e.path = root + u'/' + name + QStringLiteral(".sfc");
        if (!QFileInfo::exists(e.path))
            write(e.path, "rom " + name.toUtf8());
        e.title = name;
        e.emulatorCandidates = {QStringLiteral("fake")};
        return e;
    }

    // Starts |entry| and waits until its emulator can take a save request.
    bool startReady(const GameEntry &entry)
    {
        if (!sessions->startGame(entry, QStringLiteral("fake"), {}, true, true))
            return false;
        return QTest::qWaitFor([this] { return sessions->canSuspend(); }, Timeout);
    }

    bool suspend()
    {
        if (!sessions->suspendToHome())
            return false;
        return QTest::qWaitFor([this] { return !app->isBusy() && !sessions->hasActiveGame(); }, Timeout);
    }

    Settings settings;
    NotificationService notes;
    std::unique_ptr<AppController> app;
    std::unique_ptr<GameSessionManager> sessions;
};

class TestGameSessionManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY2(QFile::exists(QStringLiteral(NEXORA_FAKE_EMULATOR)), NEXORA_FAKE_EMULATOR); }

    void suspendSavesVerifiedStateAndResumeLoadsIt()
    {
        QTemporaryDir dir;
        QString sessionId;
        {
            Console c(dir.path());
            const GameEntry game = c.game(dir.path(), 7, QStringLiteral("Zelda"));
            QSignalSpy accepted(c.sessions.get(), &GameSessionManager::launchAccepted);
            QVERIFY(c.startReady(game));
            QCOMPARE(accepted.size(), 1);
            QCOMPARE(c.sessions->activeState(), std::optional<State>(State::Running));
            QVERIFY(c.suspend());

            QCOMPARE(c.sessions->records().size(), 1);
            const auto &record = c.sessions->records().first();
            sessionId = record.id;
            QCOMPARE(record.gameId, qint64(7));
            QCOMPARE(record.emulatorVersion, QStringLiteral("1.0"));
            QCOMPARE(record.adapter, QStringLiteral("control-socket"));
            QCOMPARE(record.states.size(), 1);
            const QString stateFile = c.sessions->sessionDirectory(record.id) + u'/' + record.states.first().file;
            QCOMPARE(read(stateFile), QByteArray("fake-state 1\n"));
            QString error;
            QCOMPARE(GameSessionManager::sha256Of(stateFile, &error), record.states.first().sha256);
            QVERIFY(QFileInfo::exists(c.sessions->sessionDirectory(record.id) + QStringLiteral("/meta.json")));
            QVERIFY(historyContains(c.notes, QStringLiteral("Zelda saved for Quick Resume")));
            QVERIFY(!c.app->manager()->lastOutcome().killed); // it quit on request
            QCOMPARE(c.sessions->suspended().size(), 1);
            QCOMPARE(c.sessions->suspended().first().toMap().value(QStringLiteral("problem")).toString(), QString());
        }

        // A new console (restart) finds the session on disk and resumes from it.
        Console c(dir.path());
        QCOMPARE(c.sessions->records().size(), 1);
        QVERIFY(c.sessions->resume(sessionId));
        QCOMPARE(c.sessions->activeState(), std::optional<State>(State::Resuming));
        QTRY_VERIFY_WITH_TIMEOUT(
            c.app->manager()->capturedStdout().contains(QStringLiteral("loaded-state: fake-state 1")), Timeout);
        QTRY_VERIFY_WITH_TIMEOUT(c.sessions->canSuspend(), Timeout);
        QVERIFY(c.suspend());
        const QStringList firstTwo = [&] {
            QStringList files;
            for (const auto &s : c.sessions->records().first().states)
                files << s.file;
            return files;
        }();
        QCOMPARE(firstTwo.size(), 2); // newest and previous
        QCOMPARE(read(c.sessions->sessionDirectory(sessionId) + u'/' + firstTwo.first()), QByteArray("fake-state 2\n"));

        // A third save drops the oldest state, only after the new one is recorded.
        const GameEntry game = c.game(dir.path(), 7, QStringLiteral("Zelda"));
        QVERIFY(c.startReady(game)); // a plain launch resumes
        QVERIFY(c.suspend());
        const auto &states = c.sessions->records().first().states;
        QCOMPARE(states.size(), 2);
        QCOMPARE(states.at(1).file, firstTwo.first());
        QVERIFY(!QFileInfo::exists(c.sessions->sessionDirectory(sessionId) + u'/' + firstTwo.last()));
    }

    void saveFailureKeepsTheGameRunning()
    {
        QTemporaryDir dir;
        Console c(dir.path(), QStringLiteral("\"--save-error\",\"disk full\""));
        QVERIFY(c.startReady(c.game(dir.path(), 1, QStringLiteral("Metroid"))));
        QVERIFY(c.sessions->suspendToHome());
        QTRY_COMPARE_WITH_TIMEOUT(c.sessions->activeState(), std::optional<State>(State::Running), Timeout);
        QVERIFY(c.app->isBusy());
        QVERIFY(historyContains(c.notes, QStringLiteral("disk full")));
        QVERIFY(c.sessions->records().isEmpty());
        QVERIFY(c.sessions->canSuspend()); // can be tried again
        c.app->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
        // The session never saved: no directory is left behind.
        QCOMPARE(QDir(dir.filePath(QStringLiteral("states"))).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
    }

    void limitsRefuseWithoutDeleting()
    {
        QTemporaryDir dir;
        Console c(dir.path());
        QVERIFY(c.settings.setValue(Settings::SessionsMaxCount, 1));
        QVERIFY(c.startReady(c.game(dir.path(), 1, QStringLiteral("First"))));
        QVERIFY(c.suspend());
        const QString firstDir = c.sessions->sessionDirectory(c.sessions->records().first().id);

        QVERIFY(c.sessions->startGame(c.game(dir.path(), 2, QStringLiteral("Second")), QStringLiteral("fake"), {}, true,
                                      true));
        QTRY_VERIFY_WITH_TIMEOUT(c.sessions->suspendUnavailableReason().contains(QStringLiteral("full")), Timeout);
        QVERIFY(!c.sessions->canSuspend());
        QVERIFY(!c.sessions->suspendToHome());
        QVERIFY(c.app->isBusy());
        QCOMPARE(c.sessions->records().size(), 1);
        QVERIFY(QFileInfo::exists(firstDir + QStringLiteral("/meta.json")));
        c.app->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
    }

    void excludedGameAndEmulatorWithoutSupport()
    {
        QTemporaryDir dir;
        {
            Console c(dir.path());
            const GameEntry game = c.game(dir.path(), 3, QStringLiteral("Excluded"));
            QVERIFY(c.sessions->setQuickResumeEnabled(3, false));
            QVERIFY(!c.sessions->isQuickResumeEnabled(3));
            QVERIFY(c.sessions->startGame(game, QStringLiteral("fake"), {}, true, true));
            QTRY_COMPARE_WITH_TIMEOUT(c.sessions->activeState(), std::optional<State>(State::Running), Timeout);
            QCOMPARE(c.sessions->suspendUnavailableReason(), QStringLiteral("Quick Resume is off for this game"));
            c.app->stop();
            QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
        }
        QTemporaryDir plain;
        Console c(plain.path(), {}, false);
        QVERIFY(c.sessions->startGame(c.game(plain.path(), 4, QStringLiteral("Plain")), QStringLiteral("fake"), {},
                                      true, true));
        QTRY_COMPARE_WITH_TIMEOUT(c.sessions->activeState(), std::optional<State>(State::Running), Timeout);
        QVERIFY(c.sessions->suspendUnavailableReason().contains(QStringLiteral("no save-state support")));
        c.app->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
    }

    void resumeIsRefusedWhenTheStateCannotBeTrusted()
    {
        QTemporaryDir dir;
        QString id;
        QString stateFile;
        GameEntry game;
        {
            Console c(dir.path());
            game = c.game(dir.path(), 5, QStringLiteral("Mario"));
            QVERIFY(c.startReady(game));
            QVERIFY(c.suspend());
            id = c.sessions->records().first().id;
            stateFile = c.sessions->sessionDirectory(id) + u'/' + c.sessions->records().first().states.first().file;
        }
        auto expectRefusal = [&](Console &c, const QString &reason) {
            QSignalSpy failed(c.sessions.get(), &GameSessionManager::resumeFailed);
            QVERIFY(!c.sessions->resume(id));
            QCOMPARE(failed.size(), 1);
            QVERIFY2(failed.first().at(2).toString().contains(reason), qPrintable(failed.first().at(2).toString()));
            QVERIFY(!c.app->isBusy());
            QVERIFY(QFileInfo::exists(stateFile)); // never removed by a refusal
        };
        {
            Console c(dir.path(), {}, true, QStringLiteral("2.0"));
            expectRefusal(c, QStringLiteral("version changed"));
            // Start fresh keeps the state and runs the game without it.
            QVERIFY(c.sessions->startFresh(id));
            QTRY_COMPARE_WITH_TIMEOUT(c.sessions->activeState(), std::optional<State>(State::Running), Timeout);
            QVERIFY(!c.app->manager()->capturedStdout().contains(QStringLiteral("loaded-state")));
            c.app->stop();
            QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
            // Played after the save: still resumable, with a note.
            QVERIFY(c.sessions->records().first().note.contains(QStringLiteral("Played after")));
            QVERIFY(QFileInfo::exists(stateFile));
        }
        {
            Console c(dir.path());
            QFile rom(game.path);
            QVERIFY(rom.open(QIODevice::Append));
            rom.write("patched");
            rom.close();
            expectRefusal(c, QStringLiteral("game file changed"));
        }
        {
            write(game.path, "rom Mario"); // same size as before, but a new modification time
            Console c(dir.path());
            write(stateFile, "fake-state 9\n"); // same size, different content
            expectRefusal(c, QStringLiteral("checksum"));
        }
    }

    void damagedMetaIsMovedAsideAndFilesKept()
    {
        QTemporaryDir dir;
        const QString sessionDir = dir.filePath(QStringLiteral("states/0b1c4d2e-0000-4000-8000-000000000001"));
        QVERIFY(QDir().mkpath(sessionDir));
        write(sessionDir + QStringLiteral("/meta.json"), "{broken");
        write(sessionDir + QStringLiteral("/fake-1-1.state"), "fake-state 1\n");
        Console c(dir.path());
        QVERIFY(c.sessions->records().isEmpty());
        QVERIFY(!QFileInfo::exists(sessionDir + QStringLiteral("/meta.json")));
        QVERIFY(QFileInfo::exists(sessionDir + QStringLiteral("/fake-1-1.state")));
        QCOMPARE(QDir(sessionDir).entryList({QStringLiteral("meta.json.corrupt-*")}, QDir::Files).size(), 1);
        QVERIFY(historyContains(c.notes, QStringLiteral("could not be read")));
    }

    void removeSessionDeletesOnlyThatSession()
    {
        QTemporaryDir dir;
        Console c(dir.path());
        QVERIFY(c.startReady(c.game(dir.path(), 1, QStringLiteral("One"))));
        QVERIFY(c.suspend());
        QVERIFY(c.startReady(c.game(dir.path(), 2, QStringLiteral("Two"))));
        QVERIFY(c.suspend());
        QCOMPARE(c.sessions->records().size(), 2);
        const QString removed = c.sessions->records().last().id;
        const QString kept = c.sessions->records().first().id;
        QVERIFY(c.sessions->removeSession(removed));
        QVERIFY(!QFileInfo::exists(c.sessions->sessionDirectory(removed)));
        QVERIFY(QFileInfo::exists(c.sessions->sessionDirectory(kept) + QStringLiteral("/meta.json")));
        QCOMPARE(c.sessions->records().size(), 1);
        QVERIFY(!c.sessions->removeSession(QStringLiteral("../not-a-session")));
    }

    void shutdownSavesTheRunningGame()
    {
        QTemporaryDir dir;
        Console c(dir.path());
        QVERIFY(c.startReady(c.game(dir.path(), 9, QStringLiteral("Shutdown"))));
        c.sessions->shutdown();
        QVERIFY(!c.sessions->hasActiveGame());
        QCOMPARE(c.sessions->records().size(), 1);
        QVERIFY(!c.app->isBusy());
    }

    void powerRequestWaitsForTheSaveOrIsDropped()
    {
        QTemporaryDir dir;
        {
            Console c(dir.path());
            QVERIFY(c.startReady(c.game(dir.path(), 1, QStringLiteral("Power"))));
            std::optional<bool> result;
            c.sessions->closeForPower([&](bool ok) { result = ok; });
            QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), Timeout);
            QVERIFY(*result);
            QVERIFY(!c.app->isBusy());
            QCOMPARE(c.sessions->records().size(), 1);
        }
        QTemporaryDir failing;
        Console c(failing.path(), QStringLiteral("\"--save-error\",\"read-only\""));
        QVERIFY(c.startReady(c.game(failing.path(), 1, QStringLiteral("Power"))));
        std::optional<bool> result;
        c.sessions->closeForPower([&](bool ok) { result = ok; });
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), Timeout);
        QVERIFY(!*result); // the power request is dropped ...
        QVERIFY(c.app->isBusy()); // ... and the game keeps running
        QVERIFY(historyContains(c.notes, QStringLiteral("Power request cancelled")));
        c.app->stop();
        QTRY_VERIFY_WITH_TIMEOUT(!c.sessions->hasActiveGame(), Timeout);
    }

    void powerRequestClosesAGameWithoutQuickResume()
    {
        QTemporaryDir dir;
        Console c(dir.path(), {}, false);
        QVERIFY(c.sessions->startGame(c.game(dir.path(), 1, QStringLiteral("Plain")), QStringLiteral("fake"), {}, true,
                                      true));
        QTRY_COMPARE_WITH_TIMEOUT(c.sessions->activeState(), std::optional<State>(State::Running), Timeout);
        std::optional<bool> result;
        c.sessions->closeForPower([&](bool ok) { result = ok; });
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), Timeout);
        QVERIFY(*result);
        QVERIFY(!c.app->isBusy());
    }

    void powerRequestClosesAnEmulatorStartedWithoutASession()
    {
        QTemporaryDir dir;
        Console c(dir.path());
        // Emulators screen: AppController starts the emulator directly, no game session exists.
        const GameEntry game = c.game(dir.path(), 1, QStringLiteral("Direct"));
        QVERIFY(c.app->launchGame(QStringLiteral("fake"), game.path, game.title));
        QTRY_VERIFY_WITH_TIMEOUT(c.app->isBusy(), Timeout);
        QVERIFY(!c.sessions->hasActiveGame());
        std::optional<bool> result;
        c.sessions->closeForPower([&](bool ok) { result = ok; });
        QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), Timeout);
        QVERIFY(*result);
        QVERIFY(!c.app->isBusy());
    }

    void differentEmulatorStartsANewSession()
    {
        QTemporaryDir dir;
        QDir().mkpath(dir.filePath(QStringLiteral("emulators")));
        Console::writeEmulator(dir.path(), {}, true, QStringLiteral("1.0"), QStringLiteral("other"));
        Console c(dir.path());
        const GameEntry game = c.game(dir.path(), 6, QStringLiteral("Twins"));
        QVERIFY(c.startReady(game));
        QVERIFY(c.suspend());
        QCOMPARE(c.sessions->records().size(), 1);
        const GameSessionManager::Record first = c.sessions->records().first();
        const QString firstState = c.sessions->sessionDirectory(first.id) + u'/' + first.states.first().file;

        // The same game on another emulator gets its own session; the first one is untouched.
        QVERIFY(c.sessions->startGame(game, QStringLiteral("other"), {}, true, true));
        QTRY_VERIFY_WITH_TIMEOUT(c.sessions->canSuspend(), Timeout);
        QVERIFY(!c.app->manager()->capturedStdout().contains(QStringLiteral("loaded-state")));
        QVERIFY(c.suspend());
        QCOMPARE(c.sessions->records().size(), 2);
        const GameSessionManager::Record second = c.sessions->records().first();
        QVERIFY(second.id != first.id);
        QCOMPARE(second.emulatorId, QStringLiteral("other"));
        QCOMPARE(second.states.size(), 1);
        const GameSessionManager::Record &kept = c.sessions->records().last();
        QCOMPARE(kept.id, first.id);
        QCOMPARE(kept.emulatorId, QStringLiteral("fake"));
        QCOMPARE(kept.states.size(), 1);
        QCOMPARE(kept.states.first().sha256, first.states.first().sha256);
        QCOMPARE(read(firstState), QByteArray("fake-state 1\n"));

        // Playing with the first emulator again resumes its own session, not the newest one.
        QVERIFY(c.startReady(game));
        QTRY_VERIFY_WITH_TIMEOUT(
            c.app->manager()->capturedStdout().contains(QStringLiteral("loaded-state: fake-state 1")), Timeout);
        QVERIFY(c.suspend());
        QCOMPARE(c.sessions->records().size(), 2);
        QCOMPARE(c.sessions->records().first().id, first.id);
        QCOMPARE(c.sessions->records().first().states.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestGameSessionManager)
#include "tst_gamesessionmanager.moc"
