// Deterministic stand-in for an emulator. Options:
//   --exit-code N      exit status (default 0)
//   --sleep-ms N       run time before exiting (default 0)
//   --crash            abort() instead of exiting normally
//   --print TEXT       print TEXT to stdout
//   --ignore-sigterm   ignore SIGTERM (exercises the launcher's kill path)
//   --write-args FILE  write all positional arguments to FILE as a JSON array
//   --                 everything after this is positional
// Positional arguments (anything else) are printed as "arg: <value>".

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    int exitCode = 0;
    int sleepMs = 0;
    bool crash = false;
    bool ignoreSigterm = false;
    QStringList printTexts;
    QString writeArgsFile;
    QStringList positional;

    const QStringList args = app.arguments().mid(1);
    bool onlyPositional = false;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &arg = args.at(i);
        const bool hasValue = i + 1 < args.size();
        if (onlyPositional) {
            positional.append(arg);
        } else if (arg == QLatin1String("--")) {
            onlyPositional = true;
        } else if (arg == QLatin1String("--exit-code") && hasValue) {
            exitCode = args.at(++i).toInt();
        } else if (arg == QLatin1String("--sleep-ms") && hasValue) {
            sleepMs = qMax(0, args.at(++i).toInt());
        } else if (arg == QLatin1String("--print") && hasValue) {
            printTexts.append(args.at(++i));
        } else if (arg == QLatin1String("--write-args") && hasValue) {
            writeArgsFile = args.at(++i);
        } else if (arg == QLatin1String("--crash")) {
            crash = true;
        } else if (arg == QLatin1String("--ignore-sigterm")) {
            ignoreSigterm = true;
        } else {
            positional.append(arg);
        }
    }

    if (ignoreSigterm)
        std::signal(SIGTERM, SIG_IGN);

    for (const QString &text : std::as_const(printTexts))
        std::printf("%s\n", qPrintable(text));
    for (const QString &arg : std::as_const(positional))
        std::printf("arg: %s\n", qPrintable(arg));
    std::fflush(stdout);

    if (!writeArgsFile.isEmpty()) {
        QFile file(writeArgsFile);
        if (!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(QJsonArray::fromStringList(positional)).toJson()) < 0) {
            std::fprintf(stderr, "mock-emulator: cannot write %s\n", qPrintable(writeArgsFile));
            return 2;
        }
    }

    QTimer::singleShot(sleepMs, &app, [&] {
        if (crash) {
            const rlimit noCore{0, 0};
            setrlimit(RLIMIT_CORE, &noCore); // no core dump for an intentional crash
            std::abort();
        }
        QCoreApplication::exit(exitCode);
    });
    return app.exec();
}
