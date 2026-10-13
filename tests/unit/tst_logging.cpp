#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class TestLogging : public QObject
{
    Q_OBJECT

private slots:
    void rotatesAndBoundsLogFiles()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath(QStringLiteral("nexora.log"));
        nexora::shared::LoggingOptions options;
        options.logFilePath = log;
        options.maxFileBytes = 4096;
        options.keepRotated = 2;
        QVERIFY(nexora::shared::installMessageHandler(options));
        const QString payload(200, u'x');
        for (int i = 0; i < 300; ++i)
            qCInfo(lcApp).noquote() << i << payload;

        QVERIFY(QFile::exists(log));
        QVERIFY(QFile::exists(log + QStringLiteral(".1")));
        QVERIFY(QFile::exists(log + QStringLiteral(".2")));
        QVERIFY(!QFile::exists(log + QStringLiteral(".3")));
        for (const QString &f : {log, log + QStringLiteral(".1"), log + QStringLiteral(".2")})
            QVERIFY2(QFileInfo(f).size() <= 4096, qPrintable(f));
        // The newest line is in the current file.
        QFile current(log);
        QVERIFY(current.open(QIODevice::ReadOnly));
        QVERIFY(current.readAll().contains(" 299 "));
        QVERIFY(nexora::shared::recentLogLines().last().contains(QStringLiteral(" 299 ")));
        qInstallMessageHandler(nullptr);
    }

    void emulatorOutputStaysOutOfTheRing()
    {
        nexora::shared::installMessageHandler({});
        qCInfo(lcApp) << "app line";
        for (int i = 0; i < 1000; ++i)
            qCInfo(lcProcessOutput) << "emulator spam";
        QVERIFY(nexora::shared::recentLogLines().last().contains(QStringLiteral("app line")));
        qInstallMessageHandler(nullptr);
    }

    void logLevelFiltersTheRingBuffer()
    {
        nexora::shared::installMessageHandler({});
        nexora::shared::setLogLevel(QtWarningMsg);
        qCInfo(lcApp) << "hidden info line";
        qCDebug(lcApp) << "hidden debug line";
        qCWarning(lcApp) << "visible warning line";
        QStringList lines = nexora::shared::recentLogLines();
        QVERIFY(lines.last().contains(QStringLiteral("visible warning line")));
        for (const QString &l : lines) {
            QVERIFY2(!l.contains(QStringLiteral("hidden info line")), qPrintable(l));
            QVERIFY2(!l.contains(QStringLiteral("hidden debug line")), qPrintable(l));
        }

        nexora::shared::setLogLevel(QtInfoMsg);
        qCDebug(lcApp) << "still hidden debug";
        qCInfo(lcApp) << "now visible info";
        lines = nexora::shared::recentLogLines();
        QVERIFY(lines.last().contains(QStringLiteral("now visible info")));
        QVERIFY(!lines.join(QLatin1Char(0x0a)).contains(QStringLiteral("still hidden debug")));

        nexora::shared::setLogLevel(QtDebugMsg);
        qCDebug(lcApp) << "debug is on";
        QVERIFY(nexora::shared::recentLogLines().last().contains(QStringLiteral("debug is on")));
        qInstallMessageHandler(nullptr);
    }
};

QTEST_GUILESS_MAIN(TestLogging)
#include "tst_logging.moc"
