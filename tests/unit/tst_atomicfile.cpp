#include "nexora/shared/AtomicFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace nexora::shared;

// Qt reports the owner bits twice (Owner and User flags); ignore the duplicate.
static constexpr QFileDevice::Permissions userBits
    = QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser;

class TestAtomicFile : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.json"));
        QString error;
        QVERIFY2(writeFileAtomically(path, "hello", &error), qPrintable(error));
        const auto data = readFileLimited(path, 1024, &error);
        QVERIFY2(data.has_value(), qPrintable(error));
        QCOMPARE(*data, QByteArray("hello"));
        // Overwrite replaces the content.
        QVERIFY(writeFileAtomically(path, "bye", &error));
        QCOMPARE(*readFileLimited(path, 1024, &error), QByteArray("bye"));
    }

    void permissionsAreOwnerReadWrite()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.json"));
        QString error;
        QVERIFY(writeFileAtomically(path, "x", &error));
        QCOMPARE(QFile::permissions(path) & ~userBits, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    void rejectsOversizeFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("big"));
        QString error;
        QVERIFY(writeFileAtomically(path, QByteArray(11, 'x'), &error));
        QVERIFY(!readFileLimited(path, 10, &error).has_value());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(readFileLimited(path, 11, &error).has_value());
    }

    void missingFileFailsWithError()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!readFileLimited(dir.filePath(QStringLiteral("nope")), 10, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void writeIntoMissingDirFailsCleanly()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!writeFileAtomically(dir.filePath(QStringLiteral("no/such/dir/f")), "x", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("no"))));
    }

    void moveAsideRenames()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("settings.json"));
        QString error;
        QVERIFY(writeFileAtomically(path, "broken", &error));
        const QString moved = moveAside(path);
        QVERIFY(!moved.isEmpty());
        QVERIFY(QFile::exists(moved));
        QVERIFY(!QFile::exists(path));
        QVERIFY(moved.startsWith(path + QStringLiteral(".corrupt-")));
        QVERIFY(moveAside(path).isEmpty()); // nothing left to move
        QVERIFY(writeFileAtomically(path, "again", &error));
        QVERIFY(moveAside(path, QStringLiteral("old")).contains(QStringLiteral(".old-")));
    }
};

QTEST_GUILESS_MAIN(TestAtomicFile)
#include "tst_atomicfile.moc"
