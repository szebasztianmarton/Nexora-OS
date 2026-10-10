#include "UiBootstrap.h"
#include "UiContext.h"
#include "input/InputController.h"

#include "nexora/shared/ConfigLoader.h"
#include "nexora/shared/Logging.h"
#include "nexora/shared/Paths.h"
#include "nexora/shared/Settings.h"
#include "nexora/system/SingleInstanceGuard.h"
#include "nexora/system/UnixSignalWatcher.h"

#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/QQmlExtensionPlugin>

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
        for (const char *opt : {"-h", "--help", "-v", "--version", "--check-config", "--help-all"}) {
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
    settings.load();
    std::printf("Settings (user file %s): %lld issue(s)\n", qPrintable(settingsPaths.userFile),
                static_cast<long long>(settings.issues().size()));
    for (const QString &issue : settings.issues()) {
        ++errors;
        std::printf("  error: %s\n", qPrintable(issue));
    }
    std::printf("%s\n", errors == 0 ? "OK" : "FAILED");
    return errors == 0 ? 0 : 1;
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
    parser.setApplicationDescription(QStringLiteral("Nexora OS console interface (MVP 2 prototype)"));
    parser.addHelpOption();
    parser.addVersionOption();
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
    parser.addOptions({configDirOption, settingsOption, databaseOption, logFileOption, inputOption, fullscreenOption,
                       windowedOption, allowRootOption, checkConfigOption});
    parser.process(app);

    const auto configDir = nexora::shared::resolveConfigDirectory(parser.value(configDirOption),
                                                                  qEnvironmentVariable(nexora::shared::ConfigDirEnvVar),
                                                                  QCoreApplication::applicationDirPath());
    const nexora::shared::Settings::Paths settingsPaths{
        nexora::shared::Settings::defaultSystemFiles(QCoreApplication::applicationDirPath()),
        parser.isSet(settingsOption) ? QDir(parser.value(settingsOption)).absolutePath()
                                     : nexora::shared::Settings::defaultUserFile()};

    if (parser.isSet(checkConfigOption))
        return checkConfiguration(configDir.path, settingsPaths);

    nexora::shared::LoggingOptions logging;
    logging.logFilePath = parser.value(logFileOption);
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
    options.app
        = {configDir.path, nexora::shared::toString(configDir.source), parser.value(logFileOption), runningAsRoot};
    options.settings = settingsPaths;
    options.databasePath = parser.isSet(databaseOption) ? QDir(parser.value(databaseOption)).absolutePath()
                                                        : nexora::library::GameLibrary::defaultDatabasePath();
    options.platformOverride = qEnvironmentVariable("NEXORA_PLATFORM");
    nexora::ui::UiContext context(options, &input);

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
