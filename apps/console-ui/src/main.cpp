#include "UiBootstrap.h"
#include "UiContext.h"
#include "input/ButtonMapping.h"
#include "input/InputController.h"

#include "nexora/shared/BuildInfo.h"
#include "nexora/shared/ConfigLoader.h"
#include "nexora/shared/DataLayout.h"
#include "nexora/shared/Logging.h"
#include "nexora/shared/Paths.h"
#include "nexora/shared/Settings.h"
#include "nexora/system/SingleInstanceGuard.h"
#include "nexora/system/SystemIntegration.h"
#include "nexora/system/UnixSignalWatcher.h"
#include "nexora/update/UpdateService.h"
#include "nexora/update/Version.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/QQmlExtensionPlugin>

#include <chrono>
#include <cstdio>
#include <cstring>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

namespace {

// Options that only print and exit must work without a display (CI, SSH, recovery shell).
void preferOffscreenForNonInteractiveRuns(int argc, char *argv[])
{
    if (qEnvironmentVariableIsSet("QT_QPA_PLATFORM") || qEnvironmentVariableIsSet("DISPLAY")
        || qEnvironmentVariableIsSet("WAYLAND_DISPLAY")) {
        return;
    }
    for (int i = 1; i < argc; ++i) {
        for (const char *opt : {"-h", "--help", "--version", "--check-config", "--help-all", "--backup", "--restore",
                                "--export-diagnostics", "--verify-update"}) {
            if (std::strcmp(argv[i], opt) == 0) {
                qputenv("QT_QPA_PLATFORM", "offscreen");
                return;
            }
        }
    }
}

// --check-config: validate emulator definitions and settings files, print a report.
int checkConfiguration(const QString &configDir, const nexora::shared::Settings::Paths &settingsPaths)
{
    using namespace nexora::shared;
    int errors = 0;
    const ConfigLoadResult result = ConfigLoader::loadDirectory(configDir);
    std::printf("Emulator definitions in %s: %lld valid\n", qPrintable(configDir),
                static_cast<long long>(result.emulators.size()));
    for (const auto &issue : result.issues) {
        const bool isError = issue.severity == ConfigIssue::Severity::Error;
        errors += isError ? 1 : 0;
        std::printf("  %s: %s\n", isError ? "error" : "warning", qPrintable(issue.toDisplayString()));
    }
    Settings settings(settingsPaths);
    settings.load(Settings::LoadMode::Inspect);
    QStringList mappingErrors;
    if (!nexora::input::ButtonMapping::fromVariantMap(settings.mapValue(Settings::InputMapping), &mappingErrors)) {
        for (const QString &e : std::as_const(mappingErrors)) {
            ++errors;
            std::printf("  error: input.mapping: %s\n", qPrintable(e));
        }
    }
    std::printf("Settings (user file %s): %lld issue(s)\n", qPrintable(settingsPaths.userFile),
                static_cast<long long>(settings.issues().size()));
    for (const QString &issue : settings.issues()) {
        ++errors;
        std::printf("  error: %s\n", qPrintable(issue));
    }
    std::printf("%s\n", errors == 0 ? "OK" : "FAILED");
    return errors == 0 ? 0 : 1;
}

// Headless actions (--backup, --restore, --export-diagnostics, --verify-update): run one
// action against a fully started (but windowless) UiContext, then exit.
int runHeadlessAction(nexora::ui::UiContext &context, const QCommandLineParser &parser,
                      const QCommandLineOption &backupOption, const QCommandLineOption &includeSavesOption,
                      const QCommandLineOption &restoreOption, const QCommandLineOption &exportDiagnosticsOption,
                      const QCommandLineOption &verifyUpdateOption)
{
    context.start();
    int rc = 0;
    if (parser.isSet(backupOption)) {
        const QString path = context.system()->createBackup(parser.isSet(includeSavesOption));
        if (path.isEmpty()) {
            std::fprintf(stderr, "backup failed: %s\n", qPrintable(context.system()->lastError()));
            rc = 1;
        } else {
            std::printf("%s\n", qPrintable(path));
        }
    } else if (parser.isSet(restoreOption)) {
        const bool ok = context.system()->restoreBackup(parser.value(restoreOption));
        if (!ok)
            std::fprintf(stderr, "restore failed: %s\n", qPrintable(context.system()->lastError()));
        else
            std::printf("restored\n");
        rc = ok ? 0 : 1;
    } else if (parser.isSet(exportDiagnosticsOption)) {
        const QString path = context.system()->exportDiagnostics();
        if (path.isEmpty()) {
            std::fprintf(stderr, "export failed: %s\n", qPrintable(context.system()->lastError()));
            rc = 1;
        } else {
            std::printf("%s\n", qPrintable(path));
        }
    } else if (parser.isSet(verifyUpdateOption)) {
        auto *updates = context.updates();
        bool ok = updates->checkBundle(parser.value(verifyUpdateOption)) && updates->verify();
        if (ok) {
            // No short timeout: hashing a 2 GiB image from a slow USB stick takes minutes.
            // Verification always ends (hash done, read error or mismatch), so the bound only
            // guards against a stuck device and is far above any real run.
            QDeadlineTimer deadline(std::chrono::hours(2));
            while (!deadline.hasExpired()
                   && (updates->state() == QStringLiteral("checking")
                       || updates->state() == QStringLiteral("verifying"))) {
                QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents, 100);
            }
            ok = updates->state() == QStringLiteral("readytoinstall");
        }
        std::printf("%s: %s\n", qPrintable(updates->state()), qPrintable(updates->message()));
        rc = ok ? 0 : 1;
    }
    context.shutdown();
    return rc;
}

} // namespace

int main(int argc, char *argv[])
{
    preferOffscreenForNonInteractiveRuns(argc, argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Nexora OS"));
    QGuiApplication::setOrganizationName(QStringLiteral("nexora"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NEXORA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Nexora OS console interface (MVP 4)"));
    parser.addHelpOption();
    const QCommandLineOption configDirOption(
        QStringLiteral("config-dir"),
        QStringLiteral("Directory containing emulator *.json definitions (overrides %1).")
            .arg(QString::fromLatin1(nexora::shared::ConfigDirEnvVar)),
        QStringLiteral("dir"));
    const QCommandLineOption settingsOption(
        QStringLiteral("settings-file"),
        QStringLiteral("User settings file (default: %1).").arg(nexora::shared::Settings::defaultUserFile()),
        QStringLiteral("file"));
    const QCommandLineOption databaseOption(
        QStringLiteral("library-db"),
        QStringLiteral("Game library database (default: %1).").arg(nexora::library::GameLibrary::defaultDatabasePath()),
        QStringLiteral("file"));
    const QCommandLineOption logFileOption(QStringLiteral("log-file"),
                                           QStringLiteral("Also append log output to <file> (rotated at 2 MiB)."),
                                           QStringLiteral("file"));
    const QCommandLineOption inputOption(QStringLiteral("input-backend"),
                                         QStringLiteral("Controller backend: auto, evdev, sdl3 or none "
                                                        "(default: the input.backend setting)."),
                                         QStringLiteral("backend"));
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("Start full screen (overrides display.mode)."));
    const QCommandLineOption windowedOption(QStringLiteral("windowed"),
                                            QStringLiteral("Start in a window (overrides display.mode)."));
    const QCommandLineOption allowRootOption(
        QStringLiteral("allow-root"),
        QStringLiteral("Allow running as root (not recommended; also NEXORA_ALLOW_ROOT=1)."));
    const QCommandLineOption checkConfigOption(
        QStringLiteral("check-config"), QStringLiteral("Validate emulator definitions and settings, then exit."));
    const QCommandLineOption versionOption(QStringLiteral("version"),
                                           QStringLiteral("Print the version, build id and board, then exit."));
    const QCommandLineOption backupOption(QStringLiteral("backup"),
                                          QStringLiteral("Create a backup (settings, library, launch history), "
                                                         "then exit."));
    const QCommandLineOption includeSavesOption(QStringLiteral("include-saves"),
                                                QStringLiteral("With --backup: also back up emulator saves."));
    const QCommandLineOption restoreOption(
        QStringLiteral("restore"), QStringLiteral("Restore the backup in <dir>, then exit."), QStringLiteral("dir"));
    const QCommandLineOption exportDiagnosticsOption(QStringLiteral("export-diagnostics"),
                                                     QStringLiteral("Write a diagnostics bundle, then exit."));
    const QCommandLineOption verifyUpdateOption(
        QStringLiteral("verify-update"),
        QStringLiteral("Check and verify the update bundle in <dir> (signature, compatibility, image hash), "
                       "then exit."),
        QStringLiteral("dir"));
    parser.addOptions({configDirOption, settingsOption, databaseOption, logFileOption, inputOption, fullscreenOption,
                       windowedOption, allowRootOption, checkConfigOption, versionOption, backupOption,
                       includeSavesOption, restoreOption, exportDiagnosticsOption, verifyUpdateOption});
    parser.process(app);

    if (parser.isSet(versionOption)) {
        std::printf("%s %s %s\n", NEXORA_VERSION, nexora::shared::build::BuildId, nexora::shared::build::Board);
        return 0;
    }

    const auto configDir = nexora::shared::resolveConfigDirectory(parser.value(configDirOption),
                                                                  qEnvironmentVariable(nexora::shared::ConfigDirEnvVar),
                                                                  QCoreApplication::applicationDirPath());
    const nexora::shared::Settings::Paths settingsPaths{
        nexora::shared::Settings::defaultSystemFiles(QCoreApplication::applicationDirPath()),
        parser.isSet(settingsOption) ? QDir(parser.value(settingsOption)).absolutePath()
                                     : nexora::shared::Settings::defaultUserFile()};

    if (parser.isSet(checkConfigOption))
        return checkConfiguration(configDir.path, settingsPaths);

    const auto layout = nexora::shared::DataLayout::fromEnvironment();
    const QString platformOverride = qEnvironmentVariable("NEXORA_PLATFORM");
    const nexora::system::SystemEnvironment sysEnv;
    const bool embedded = nexora::system::SystemIntegration::detectPlatform(sysEnv, platformOverride)
        == nexora::system::Platform::Embedded;

    nexora::shared::LoggingOptions logging;
    logging.logFilePath = parser.isSet(logFileOption)
        ? parser.value(logFileOption)
        : (embedded ? layout.logDir + QStringLiteral("/console-ui.log") : QString());
    if (!logging.logFilePath.isEmpty())
        QDir().mkpath(QFileInfo(logging.logFilePath).absolutePath());
    nexora::shared::installMessageHandler(logging);

    const bool runningAsRoot = nexora::shared::isRunningAsRoot();
    if (runningAsRoot) {
        const bool allowed
            = parser.isSet(allowRootOption) || qEnvironmentVariable("NEXORA_ALLOW_ROOT") == QLatin1String("1");
        if (!allowed) {
            qCCritical(lcApp) << "Refusing to run as root: emulators would inherit root privileges."
                              << "Run as a regular user, or pass --allow-root if you really must.";
            return 2;
        }
        qCWarning(lcApp) << "Running as root (--allow-root). Emulators inherit root privileges.";
    }

    nexora::system::SingleInstanceGuard instance;
    if (!instance.tryAcquire()) {
        qCCritical(lcApp) << "Nexora is already running (pid" << instance.ownerPid() << ", lock" << instance.lockPath()
                          << ")";
        return 3;
    }

    qCInfo(lcApp).noquote() << "Nexora OS" << NEXORA_VERSION << "- configuration directory:" << configDir.path << "("
                            << nexora::shared::toString(configDir.source) << ")";

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    nexora::input::InputController input;
    nexora::ui::UiContext::Options options;
    options.app = {configDir.path, nexora::shared::toString(configDir.source), parser.value(logFileOption),
                   runningAsRoot, nexora::shared::defaultStateDirectory() + QStringLiteral("/emulator-logs")};
    options.settings = settingsPaths;
    options.layout = layout;
    options.databasePath = parser.isSet(databaseOption) ? QDir(parser.value(databaseOption)).absolutePath() : QString();
    options.platformOverride = platformOverride;
    options.environment = sysEnv;
    options.update.keyDir
        = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../share/nexora/update-keys"));
    options.update.stagingDir = layout.stateDir + QStringLiteral("/update");
    options.update.currentVersion
        = nexora::update::Version::parse(QStringLiteral(NEXORA_VERSION)).value_or(nexora::update::Version{});
    options.update.currentBuildId = QString::fromLatin1(nexora::shared::build::BuildId);
    options.update.board = QString::fromLatin1(nexora::shared::build::Board);
    nexora::ui::UiContext context(options, &input);

    if (parser.isSet(backupOption) || parser.isSet(restoreOption) || parser.isSet(exportDiagnosticsOption)
        || parser.isSet(verifyUpdateOption)) {
        return runHeadlessAction(context, parser, backupOption, includeSavesOption, restoreOption,
                                 exportDiagnosticsOption, verifyUpdateOption);
    }

    const QString backend = parser.isSet(inputOption)
        ? parser.value(inputOption)
        : context.settingsStore().stringValue(nexora::shared::Settings::InputBackend);
    if (!input.start(backend))
        qCWarning(lcApp) << "Controller input unavailable; keyboard navigation still works";
    context.start();

    nexora::system::UnixSignalWatcher signalWatcher;
    QObject::connect(&signalWatcher, &nexora::system::UnixSignalWatcher::signalReceived, &app, [](int sig) {
        qCInfo(lcApp) << "Termination signal" << sig << "- shutting down";
        QCoreApplication::quit();
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &context, [&context] { context.shutdown(); });

    QQmlApplicationEngine engine;
    QQuickWindow *window = nexora::ui::loadMainWindow(engine, &context);
    if (!window)
        return 1;

    auto applyDisplayMode = [window](const QString &mode) {
        if (mode == QLatin1String("fullscreen"))
            window->showFullScreen();
        else
            window->showNormal();
        // Controller input is delivered to the focused window; make sure we have focus
        // (e.g. on EGLFS/KMS there is no window manager to give it to us).
        window->requestActivate();
    };
    if (parser.isSet(fullscreenOption) || parser.isSet(windowedOption)) {
        applyDisplayMode(parser.isSet(fullscreenOption) ? QStringLiteral("fullscreen") : QStringLiteral("windowed"));
    } else {
        const QString resolution = context.settingsStore().stringValue(nexora::shared::Settings::DisplayResolution);
        if (resolution != QLatin1String("auto"))
            window->resize(resolution.section(u'x', 0, 0).toInt(), resolution.section(u'x', 1, 1).toInt());
        applyDisplayMode(context.settingsStore().stringValue(nexora::shared::Settings::DisplayMode));
        QObject::connect(context.settings(), &nexora::ui::SettingsController::displayModeChanged, window,
                         applyDisplayMode);
    }

    return app.exec();
}
