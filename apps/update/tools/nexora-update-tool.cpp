// Stand-alone bundle verifier: checks a signed update bundle's signature, compatibility
// and image hash without touching the staging directory, the install FIFO or any running
// UpdateService. Useful for CI and for release scripts sanity-checking a bundle before it
// ships. "sign" is deliberately not a subcommand here; see CMakeLists.txt.
#include "nexora/shared/AtomicFile.h"
#include "nexora/update/SignatureVerifier.h"
#include "nexora/update/UpdateManifest.h"
#include "nexora/update/UpdateService.h"
#include "nexora/update/Version.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTextStream>

#include <cstdio>

using namespace nexora::update;

namespace {

constexpr const char *ExpectedProduct = "nexora-os";

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

    const QString dir = QFileInfo(bundleDir).absoluteFilePath();
    QString readError;
    const auto manifestBytes
        = nexora::shared::readFileLimited(dir + QStringLiteral("/manifest.json"), 64 * 1024, &readError);
    if (!manifestBytes) {
        printError(UpdateError::BadManifest, readError);
        return 1;
    }

    QString parseError;
    const auto manifest = UpdateManifest::parse(*manifestBytes, &parseError);
    if (!manifest) {
        printError(UpdateError::BadManifest, parseError);
        return 1;
    }

    QString sigError;
    const auto signature = nexora::shared::readFileLimited(dir + QStringLiteral("/manifest.json.sig"), 64, &sigError);
    if (!signature || signature->size() != 64) {
        printError(UpdateError::BadSignature,
                   sigError.isEmpty() ? QStringLiteral("signature must be 64 bytes") : sigError);
        return 1;
    }

    QStringList issues;
    const SignatureVerifier verifier = SignatureVerifier::fromDirectory(keysDir, &issues);
    if (verifier.keyCount() == 0) {
        printError(UpdateError::NoKeys, issues.join(QStringLiteral("; ")));
        return 1;
    }
    if (!verifier.verify(*manifestBytes, *signature)) {
        printError(UpdateError::BadSignature, {});
        return 1;
    }

    if (manifest->product != QLatin1String(ExpectedProduct)) {
        printError(UpdateError::WrongProduct, manifest->product);
        return 1;
    }
    // --board and --current-version are optional: without them this tool only answers
    // "is this bundle authentic and intact", not "is it applicable to this machine".
    if (!board.isEmpty() && manifest->board != board) {
        printError(UpdateError::WrongBoard, manifest->board);
        return 1;
    }
    if (!currentVersionText.isEmpty()) {
        const auto currentVersion = Version::parse(currentVersionText);
        if (!currentVersion) {
            std::fprintf(stderr, "invalid --current-version\n");
            return 1;
        }
        if (!(*currentVersion < manifest->version)) {
            printError(UpdateError::NotNewer, manifest->version.toString());
            return 1;
        }
        if (*currentVersion < manifest->minimumVersion) {
            printError(UpdateError::TooOld, manifest->minimumVersion.toString());
            return 1;
        }
    }

    const QString imagePath = dir + u'/' + manifest->image.file;
    const QFileInfo imageInfo(imagePath);
    if (!imageInfo.isFile()) {
        printError(UpdateError::ImageMissing, manifest->image.file);
        return 1;
    }
    if (imageInfo.size() != manifest->image.size) {
        printError(UpdateError::SizeMismatch,
                   QStringLiteral("expected %1, found %2").arg(manifest->image.size).arg(imageInfo.size()));
        return 1;
    }

    QString hashError;
    const auto digest = sha256File(imagePath, {}, nullptr, &hashError);
    if (!digest) {
        printError(UpdateError::HashMismatch, hashError);
        return 1;
    }
    if (*digest != manifest->image.sha256Hex) {
        printError(UpdateError::HashMismatch, {});
        return 1;
    }

    QTextStream out(stdout);
    out << QStringLiteral("ok %1 %2\n").arg(manifest->version.toString(), QString::fromLatin1(*digest));
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
