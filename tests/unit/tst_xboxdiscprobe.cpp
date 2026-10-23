// XboxDiscProbe against synthetic sparse disc images.
#include "nexora/library/XboxDiscProbe.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using nexora::library::isXboxDisc;

class TestXboxDiscProbe : public QObject
{
    Q_OBJECT

private:
    static constexpr const char *Magic = "MICROSOFT*XBOX*MEDIA";

    // Writes `bytes` at `offset` and makes the file `size` bytes long (sparse).
    static QString image(const QTemporaryDir &dir, const QString &name, qint64 offset, const QByteArray &bytes,
                         qint64 size)
    {
        const QString path = dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) || !f.resize(size) || !f.seek(offset) || f.write(bytes) != bytes.size())
            qFatal("cannot write test image");
        return path;
    }

private slots:
    void detectsSignatureAtEveryBase_data()
    {
        QTest::addColumn<qint64>("base");
        QTest::newRow("xiso") << qint64(0x0);
        QTest::newRow("xgd3") << qint64(0x2080000);
        QTest::newRow("xgd2") << qint64(0xFD90000);
    }

    void detectsSignatureAtEveryBase()
    {
        QFETCH(qint64, base);
        QTemporaryDir dir;
        const QString path = image(dir, QStringLiteral("game.iso"), base + 0x10000, Magic, base + 0x20000);
        QVERIFY(isXboxDisc(path));
    }

    void rejectsShiftedSignature()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("a.iso"), 0x10001, Magic, 0x20000)));
    }

    void rejectsTruncatedSignature()
    {
        QTemporaryDir dir;
        // Only the first 10 bytes of the magic fit before the end of the file.
        const QString path = dir.filePath(QStringLiteral("t.iso"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.resize(0x10000));
        QVERIFY(f.seek(0x10000));
        f.write(QByteArray(Magic).left(10));
        f.close();
        QVERIFY(!isXboxDisc(path));
    }

    void rejectsIso9660()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("ps2.iso"), 0x8001, "CD001", 0x100000)));
    }

    void rejectsEmptyAndMissingFiles()
    {
        QTemporaryDir dir;
        QVERIFY(!isXboxDisc(image(dir, QStringLiteral("empty.iso"), 0, QByteArray(), 0)));
        QVERIFY(!isXboxDisc(dir.filePath(QStringLiteral("missing.iso"))));
    }
};

QTEST_GUILESS_MAIN(TestXboxDiscProbe)
#include "tst_xboxdiscprobe.moc"
