// Save-state adapters against real processes: the control-socket adapter with
// nexora-fake-emulator, the RetroArch adapter with a stand-in UDP listener (RetroArch itself
// is not integrated yet, so that adapter stays unverified).
#include "nexora/emulator/EmulatorManager.h"
#include "nexora/emulator/SaveStateAdapter.h"

#include <QDir>
#include <QFile>
#include <QLocalSocket>
#include <QNetworkDatagram>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUdpSocket>

using namespace nexora::emulator;
using nexora::shared::EmulatorConfig;
using nexora::shared::SaveStateCapability;

namespace {

SaveStateCapability controlSocket(int timeoutMs = 5000)
{
    SaveStateCapability cap;
    cap.adapter = SaveStateCapability::Adapter::ControlSocket;
    cap.timeoutMs = timeoutMs;
    cap.launchArguments = {QStringLiteral("--control-socket"), QStringLiteral("{control-socket}")};
    cap.resumeArguments = {QStringLiteral("--load-state"), QStringLiteral("{state-file}")};
    return cap;
}

QByteArray readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
}

} // namespace

class TestSaveStateAdapter : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_game;

    // Starts the fake emulator with |adapter|'s launch arguments and waits for it to connect.
    bool start(EmulatorManager &manager, ControlSocketAdapter &adapter, const QStringList &fakeOptions,
               const QString &resumeFile = {})
    {
        QString error;
        const auto extra = adapter.prepareLaunch({m_dir.path(), resumeFile, m_game}, &error);
        if (!extra) {
            qWarning() << error;
            return false;
        }
        QSignalSpy ready(&adapter, &SaveStateAdapter::readyChanged);
        const auto config = EmulatorConfig::make(QStringLiteral("fake"), QStringLiteral("Fake"),
                                                 QStringLiteral(NEXORA_FAKE_EMULATOR), fakeOptions);
        if (!manager.launch(LaunchRequest{config, {}, m_game, {}, *extra}).accepted)
            return false;
        return adapter.isReady() || ready.wait(5000);
    }

    static QStringList sleeping(QStringList options = {})
    {
        return options << QStringLiteral("--sleep-ms") << QStringLiteral("20000");
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(QFile::exists(QStringLiteral(NEXORA_FAKE_EMULATOR)), NEXORA_FAKE_EMULATOR);
        m_game = m_dir.filePath(QStringLiteral("Super Game.sfc"));
        writeFile(m_game, "rom");
    }

    void controlSocketSavesQuitsAndResumes()
    {
        const QString states = m_dir.filePath(QStringLiteral("states-a"));
        QVERIFY(QDir().mkpath(states));
        QString file;
        {
            EmulatorManager manager;
            ControlSocketAdapter adapter(controlSocket());
            QVERIFY(start(manager, adapter, sleeping()));
            QVERIFY(QFileInfo(adapter.socketPath()).exists());

            QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
            adapter.save(states);
            QVERIFY(saved.wait(5000));
            QVERIFY2(saved.first().at(0).toBool(), qPrintable(saved.first().at(2).toString()));
            file = saved.first().at(1).toString();
            QVERIFY(file.startsWith(states + u'/'));
            QCOMPARE(readAll(file), QByteArray("fake-state 1\n"));
            QVERIFY(manager.isActive()); // saving does not end the game

            QSignalSpy quit(&adapter, &SaveStateAdapter::quitFinished);
            QSignalSpy finished(&manager, &EmulatorManager::processFinished);
            adapter.requestQuit();
            QVERIFY(!quit.isEmpty() || quit.wait(5000));
            QVERIFY(quit.first().at(0).toBool());
            QVERIFY(!finished.isEmpty() || finished.wait(5000)); // may already have exited
            QCOMPARE(manager.lastOutcome().exitCode, std::optional<int>(0));
            QVERIFY(!manager.lastOutcome().stoppedByRequest);
        }

        EmulatorManager manager;
        ControlSocketAdapter adapter(controlSocket());
        QVERIFY(start(manager, adapter, sleeping(), file));
        QTRY_VERIFY(manager.capturedStdout().contains(QStringLiteral("loaded-state: fake-state 1")));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(states);
        QVERIFY(saved.wait(5000));
        const QString second = saved.first().at(1).toString();
        QVERIFY(second != file);
        QCOMPARE(readAll(second), QByteArray("fake-state 2\n"));
        QCOMPARE(readAll(file), QByteArray("fake-state 1\n")); // the earlier state is untouched
        manager.stop();
        QTRY_VERIFY(!manager.isActive());
    }

    void emulatorErrorsAreReported_data()
    {
        QTest::addColumn<QStringList>("options");
        QTest::addColumn<QString>("expected");
        QTest::newRow("ERR answer") << QStringList{QStringLiteral("--save-error"), QStringLiteral("disk full")}
                                    << QStringLiteral("disk full");
        QTest::newRow("empty file") << QStringList{QStringLiteral("--save-empty")} << QStringLiteral("empty");
    }

    void emulatorErrorsAreReported()
    {
        QFETCH(QStringList, options);
        QFETCH(QString, expected);
        EmulatorManager manager;
        ControlSocketAdapter adapter(controlSocket());
        QVERIFY(start(manager, adapter, sleeping(options)));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(m_dir.path());
        QVERIFY(saved.wait(5000));
        QVERIFY(!saved.first().at(0).toBool());
        QVERIFY2(saved.first().at(2).toString().contains(expected), qPrintable(saved.first().at(2).toString()));
        QVERIFY(manager.isActive()); // a failed save never ends the game
        manager.stop();
        QTRY_VERIFY(!manager.isActive());
    }

    void slowAnswerTimesOutAndOneRequestAtATime()
    {
        EmulatorManager manager;
        ControlSocketAdapter adapter(controlSocket(1000));
        QVERIFY(start(manager, adapter, sleeping({QStringLiteral("--save-delay-ms"), QStringLiteral("3000")})));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(m_dir.path());
        adapter.save(m_dir.path()); // refused at once while the first one runs
        QCOMPARE(saved.size(), 1);
        QVERIFY(saved.first().at(2).toString().contains(QStringLiteral("still running")));
        QVERIFY(saved.wait(3000));
        QVERIFY(!saved.at(1).at(0).toBool());
        QVERIFY(saved.at(1).at(2).toString().contains(QStringLiteral("did not answer")));
        QVERIFY(!adapter.isBusy());
        manager.stop();
        QTRY_VERIFY(!manager.isActive());
    }

    void requestsWithoutAConnectedEmulatorFail()
    {
        ControlSocketAdapter adapter(controlSocket());
        QString error;
        QVERIFY(adapter.prepareLaunch({m_dir.path(), {}, m_game}, &error));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(m_dir.path());
        QCOMPARE(saved.size(), 1);
        QVERIFY(saved.first().at(2).toString().contains(QStringLiteral("not connected")));
        QSignalSpy quit(&adapter, &SaveStateAdapter::quitFinished);
        adapter.requestQuit();
        QCOMPARE(quit.size(), 1);
        QVERIFY(!quit.first().at(0).toBool());

        QVERIFY(
            !adapter.prepareLaunch({m_dir.path(), m_dir.filePath(QStringLiteral("missing.state")), m_game}, &error));
        QVERIFY(error.contains(QStringLiteral("does not exist")));
    }

    void refusedQuitIsReported()
    {
        EmulatorManager manager;
        ControlSocketAdapter adapter(controlSocket());
        QVERIFY(start(manager, adapter, sleeping({QStringLiteral("--ignore-quit")})));
        QSignalSpy quit(&adapter, &SaveStateAdapter::quitFinished);
        adapter.requestQuit();
        QVERIFY(!quit.isEmpty() || quit.wait(5000));
        QVERIFY(!quit.first().at(0).toBool());
        QVERIFY(manager.isActive()); // the caller falls back to SIGTERM
        manager.stop();
        QTRY_VERIFY(!manager.isActive());
    }

    void reportedStateFileMustBeDirectlyInTheDirectory_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("expected");
        QTest::newRow("subdirectory") << QStringLiteral("sub/nested.state") << QStringLiteral("directly");
        QTest::newRow("meta.json") << QStringLiteral("meta.json") << QStringLiteral("meta.json");
    }

    // A stand-in on the control socket answers SAVE with a file that is inside the session
    // directory but not acceptable as a state.
    void reportedStateFileMustBeDirectlyInTheDirectory()
    {
        QFETCH(QString, name);
        QFETCH(QString, expected);
        const QString states = m_dir.filePath(QStringLiteral("states-") + QString::fromLatin1(QTest::currentDataTag()));
        QVERIFY(QDir().mkpath(states + QStringLiteral("/sub")));
        ControlSocketAdapter adapter(controlSocket());
        QString error;
        QVERIFY(adapter.prepareLaunch({m_dir.path(), {}, m_game}, &error));
        QLocalSocket emulator;
        emulator.connectToServer(adapter.socketPath());
        QVERIFY(emulator.waitForConnected(5000));
        QTRY_VERIFY(adapter.isReady());
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(states);
        QTRY_VERIFY(emulator.bytesAvailable() > 0);
        QVERIFY(emulator.readAll().startsWith("SAVE "));
        const QString file = states + u'/' + name;
        writeFile(file, "not a state");
        emulator.write("OK " + file.toUtf8() + "\n");
        QVERIFY(saved.wait(5000));
        QVERIFY(!saved.first().at(0).toBool());
        QVERIFY2(saved.first().at(2).toString().contains(expected), qPrintable(saved.first().at(2).toString()));
    }

    void controlSocketClosedDuringSaveFails()
    {
        EmulatorManager manager;
        ControlSocketAdapter adapter(controlSocket());
        QVERIFY(start(manager, adapter, sleeping({QStringLiteral("--save-delay-ms"), QStringLiteral("3000")})));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(m_dir.path());
        manager.stop();
        QVERIFY(saved.wait(5000));
        QVERIFY(!saved.first().at(0).toBool());
        QVERIFY(saved.first().at(2).toString().contains(QStringLiteral("closed")));
        QTRY_VERIFY(!manager.isActive());
    }

    // RetroArch protocol against a stand-in: UNVERIFIED with RetroArch itself.
    void retroArchProtocolWithStandIn()
    {
        QUdpSocket standIn;
        QVERIFY(standIn.bind(QHostAddress::LocalHost, 0));
        SaveStateCapability cap;
        cap.adapter = SaveStateCapability::Adapter::RetroArch;
        cap.timeoutMs = 3000;
        cap.port = standIn.localPort();
        const QString session = m_dir.filePath(QStringLiteral("ra-session"));
        const QString states = m_dir.filePath(QStringLiteral("ra-states"));
        QVERIFY(QDir().mkpath(session));
        QVERIFY(QDir().mkpath(states));

        RetroArchAdapter adapter(cap);
        QString error;
        const auto args = adapter.prepareLaunch({session, {}, m_game}, &error);
        QVERIFY2(args, qPrintable(error));
        const QString cfg = session + u'/' + RetroArchAdapter::configFileName();
        QCOMPARE(*args, QStringList({QStringLiteral("--appendconfig"), cfg}));
        const QByteArray config = readAll(cfg);
        QVERIFY(config.contains("network_cmd_port = \"" + QByteArray::number(cap.port) + '"'));
        QVERIFY(config.contains("savestate_directory = \"" + adapter.workDirectory().toUtf8() + '"'));

        // When SAVE_STATE arrives, "RetroArch" writes its slot file.
        const QString slot = adapter.workDirectory() + QStringLiteral("/Super Game.state");
        connect(&standIn, &QUdpSocket::readyRead, this, [&] {
            while (standIn.hasPendingDatagrams()) {
                if (standIn.receiveDatagram().data() == "SAVE_STATE\n")
                    writeFile(slot, "ra-state-1");
            }
        });
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(states);
        QVERIFY(saved.wait(4000));
        QVERIFY2(saved.first().at(0).toBool(), qPrintable(saved.first().at(2).toString()));
        const QString copy = saved.first().at(1).toString();
        QVERIFY(copy.startsWith(states + u'/'));
        QCOMPARE(readAll(copy), QByteArray("ra-state-1"));

        // Resume: the state is copied to the slot RetroArch loads from and --entryslot 0 added.
        QFile::remove(slot);
        const auto resume = adapter.prepareLaunch({session, copy, m_game}, &error);
        QVERIFY2(resume, qPrintable(error));
        QVERIFY(resume->contains(QStringLiteral("--entryslot")));
        QCOMPARE(readAll(slot), QByteArray("ra-state-1"));
        QCOMPARE(readAll(copy), QByteArray("ra-state-1"));

        standIn.disconnect(this);
        QSignalSpy quit(&adapter, &SaveStateAdapter::quitFinished);
        adapter.requestQuit();
        QCOMPARE(quit.size(), 1);
        QVERIFY(quit.first().at(0).toBool());
        QTRY_VERIFY(standIn.hasPendingDatagrams());
        QCOMPARE(standIn.receiveDatagram().data(), QByteArray("QUIT\n"));
    }

    void retroArchWithoutAStateTimesOut()
    {
        SaveStateCapability cap;
        cap.adapter = SaveStateCapability::Adapter::RetroArch;
        cap.timeoutMs = 1000;
        cap.port = 1; // nothing listens; UDP sends still succeed
        const QString session = m_dir.filePath(QStringLiteral("ra-timeout"));
        QVERIFY(QDir().mkpath(session));
        RetroArchAdapter adapter(cap);
        QString error;
        QVERIFY(adapter.prepareLaunch({session, {}, m_game}, &error));
        QSignalSpy saved(&adapter, &SaveStateAdapter::saveFinished);
        adapter.save(session);
        QVERIFY(saved.wait(3000));
        QVERIFY(!saved.first().at(0).toBool());
        QVERIFY(saved.first().at(2).toString().contains(QStringLiteral("did not write")));
    }
};

QTEST_GUILESS_MAIN(TestSaveStateAdapter)
#include "tst_savestateadapter.moc"
