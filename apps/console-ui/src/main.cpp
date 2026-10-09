#include "AppController.h"
#include "UiBootstrap.h"
#include "input/InputController.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/Paths.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtQml/QQmlExtensionPlugin>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Nexora OS"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NEXORA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Nexora OS console interface (MVP 1 prototype)"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption configDirOption(
        QStringLiteral("config-dir"),
        QStringLiteral("Directory containing emulator *.json definitions (overrides %1).")
            .arg(QString::fromLatin1(nexora::shared::ConfigDirEnvVar)),
        QStringLiteral("dir"));
    const QCommandLineOption logFileOption(QStringLiteral("log-file"),
                                           QStringLiteral("Also append log output to <file>."),
                                           QStringLiteral("file"));
    const QCommandLineOption inputOption(QStringLiteral("input-backend"),
                                         QStringLiteral("Controller backend: auto, evdev, sdl3 or none."),
                                         QStringLiteral("backend"), QStringLiteral("auto"));
    const QCommandLineOption fullscreenOption(QStringLiteral("fullscreen"),
                                              QStringLiteral("Show the interface full screen."));
    const QCommandLineOption allowRootOption(
        QStringLiteral("allow-root"),
        QStringLiteral("Allow running as root (not recommended; also NEXORA_ALLOW_ROOT=1)."));
    parser.addOptions({configDirOption, logFileOption, inputOption, fullscreenOption, allowRootOption});
    parser.process(app);

    nexora::shared::installMessageHandler({parser.value(logFileOption), 500});

    const bool runningAsRoot = nexora::shared::isRunningAsRoot();
    if (runningAsRoot) {
        const bool allowed = parser.isSet(allowRootOption)
                             || qEnvironmentVariable("NEXORA_ALLOW_ROOT") == QLatin1String("1");
        if (!allowed) {
            qCCritical(lcApp) << "Refusing to run as root: emulators would inherit root privileges."
                              << "Run as a regular user, or pass --allow-root if you really must.";
            return 2;
        }
        qCWarning(lcApp) << "Running as root (--allow-root). Emulators inherit root privileges.";
    }

    const auto configDir = nexora::shared::resolveConfigDirectory(
        parser.value(configDirOption),
        qEnvironmentVariable(nexora::shared::ConfigDirEnvVar),
        QCoreApplication::applicationDirPath());
    qCInfo(lcApp).noquote() << "Nexora OS" << NEXORA_VERSION << "- configuration directory:"
                            << configDir.path << "(" << nexora::shared::toString(configDir.source) << ")";

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    nexora::ui::AppController controller({configDir.path,
                                          nexora::shared::toString(configDir.source),
                                          parser.value(logFileOption), runningAsRoot});

    nexora::input::InputController input;
    if (!input.start(parser.value(inputOption)))
        qCWarning(lcApp) << "Controller input unavailable; keyboard navigation still works";
    controller.setInputController(&input);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller,
                     [&controller] { controller.manager()->shutdown(); });

    QQmlApplicationEngine engine;
    QQuickWindow *window = nexora::ui::loadMainWindow(engine, &controller);
    if (!window)
        return 1;
    if (parser.isSet(fullscreenOption))
        window->showFullScreen();

    return app.exec();
}
