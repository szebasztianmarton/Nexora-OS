#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>

#include "backend.h"
#include "gamelibrary.h"
#include "logger.h"

Q_IMPORT_QML_PLUGIN(NexoraPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("nexora"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Nexora OS"));
    QGuiApplication::setOrganizationName(QStringLiteral("nexora"));
    QGuiApplication::setApplicationVersion(QStringLiteral(NEXORA_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Nexora OS console interface"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption configOption(QStringLiteral("config-dir"),
                                    QStringLiteral("Configuration directory (default: $NEXORA_CONFIG_DIR or ~/.config/nexora)."),
                                    QStringLiteral("path"));
    QCommandLineOption logOption(QStringLiteral("log-dir"),
                                 QStringLiteral("Log directory (default: $NEXORA_LOG_DIR or ~/.local/share/nexora/logs)."),
                                 QStringLiteral("path"));
    QCommandLineOption windowedOption(QStringLiteral("windowed"),
                                      QStringLiteral("Start in a window instead of full screen (development)."));
    QCommandLineOption sizeOption(QStringLiteral("size"),
                                  QStringLiteral("Initial window size for --windowed, e.g. 1280x720."),
                                  QStringLiteral("WxH"));
    parser.addOptions({configOption, logOption, windowedOption, sizeOption});
    parser.process(app);

    auto pick = [&](const QCommandLineOption &option, const char *env, const QString &fallback) {
        if (parser.isSet(option))
            return QDir(parser.value(option)).absolutePath();
        const QString fromEnv = qEnvironmentVariable(env);
        return fromEnv.isEmpty() ? fallback : QDir(fromEnv).absolutePath();
    };
    const QString configDir = pick(configOption, "NEXORA_CONFIG_DIR",
                                   QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    const QString logDir = pick(logOption, "NEXORA_LOG_DIR",
                                QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                    + QStringLiteral("/logs"));
    QDir().mkpath(configDir);
    nexora::Logger::init(logDir);
    qInfo("Nexora OS %s (%s) starting, config: %s", NEXORA_VERSION, NEXORA_BUILD_ID, qPrintable(configDir));

    QFont font(QStringLiteral("Inter"));
    font.setFamilies({QStringLiteral("Inter"), QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans"),
                      QStringLiteral("sans-serif")});
    QGuiApplication::setFont(font);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    nexora::Backend backend(configDir);
    backend.registerQmlTypes();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("nexoraWindowed"), parser.isSet(windowedOption));
    QSize initialSize(1920, 1080);
    if (parser.isSet(sizeOption)) {
        const QStringList parts = parser.value(sizeOption).split(QLatin1Char('x'));
        if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0)
            initialSize = QSize(parts[0].toInt(), parts[1].toInt());
    }
    engine.rootContext()->setContextProperty(QStringLiteral("nexoraInitialSize"), initialSize);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Nexora/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        backend.attachWindow(window);

    backend.library()->refresh();
    const int rc = app.exec();
    nexora::Logger::shutdown();
    return rc;
}
