// Stand-alone bundle verifier: checks a signed update bundle's signature, compatibility
// and image hash without touching the staging directory, the install FIFO or any running
// UpdateService. Useful for CI and for release scripts sanity-checking a bundle before it
// ships. "sign" is deliberately not a subcommand here; see CMakeLists.txt.
#include "nexora/update/SignatureVerifier.h"
#include "nexora/update/UpdateService.h"
#include "nexora/update/Version.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>
#include <optional>

using namespace nexora::update;

namespace {

void printUsage()
{
    std::fprintf(stderr,
                 "usage: nexora-update-tool verify --keys DIR --bundle DIR "
                 "[--current-version X --board B]\n");
}

void printError(UpdateError error, const QString &detail)
{
    QTextStream out(stdout);
    out << QStringLiteral("error %1 %2\n").arg(nameOf(error), detail.isEmpty() ? describe(error) : detail);
}

int runVerify(const QStringList &args)
{
    QString keysDir, bundleDir, currentVersionText, board;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        auto next = [&]() -> QString { return (i + 1 < args.size()) ? args.at(++i) : QString(); };
        if (a == QStringLiteral("--keys"))
            keysDir = next();
        else if (a == QStringLiteral("--bundle"))
            bundleDir = next();
        else if (a == QStringLiteral("--current-version"))
            currentVersionText = next();
        else if (a == QStringLiteral("--board"))
            board = next();
        else {
            std::fprintf(stderr, "unknown argument: %s\n", qPrintable(a));
            printUsage();
            return 1;
        }
    }
    if (keysDir.isEmpty() || bundleDir.isEmpty()) {
        printUsage();
        return 1;
    }

    // --board and --current-version are optional: without them this tool only answers
    // "is this bundle authentic and intact", not "is it applicable to this machine".
    std::optional<Version> currentVersion;
    if (!currentVersionText.isEmpty()) {
        currentVersion = Version::parse(currentVersionText);
        if (!currentVersion) {
            std::fprintf(stderr, "invalid --current-version\n");
            return 1;
        }
    }
    const std::optional<QString> boardOpt = board.isEmpty() ? std::nullopt : std::optional<QString>(board);

    UpdateError error = UpdateError::None;
    QString detail;
    const auto verified = verifyBundle(bundleDir, keysDir, boardOpt, currentVersion, &error, &detail);
    if (!verified) {
        printError(error, detail);
        return 1;
    }

    QString hashError;
    const auto digest = sha256File(verified->imagePath, {}, nullptr, &hashError);
    if (!digest) {
        printError(UpdateError::HashMismatch, hashError);
        return 1;
    }
    if (*digest != verified->manifest.image.sha256Hex) {
        printError(UpdateError::HashMismatch, {});
        return 1;
    }

    QTextStream out(stdout);
    out << QStringLiteral("ok %1 %2\n").arg(verified->manifest.version.toString(), QString::fromLatin1(*digest));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2 || args.at(1) != QStringLiteral("verify")) {
        printUsage();
        return 1;
    }
    return runVerify(args.mid(2));
}
