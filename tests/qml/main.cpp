#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQuickTest>

#include "backend.h"
#include "gamelibrary.h"
#include "logger.h"

Q_IMPORT_QML_PLUGIN(NexoraPlugin)

namespace {

void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
}

} // namespace

// Builds a sandbox (config + library) before the QML engine starts and
// exposes the backend exactly as the application does.
class Setup : public QObject
{
    Q_OBJECT
public slots:
    // QuickTest constructs this object before QGuiApplication exists, so all
    // setup happens here.
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        const QString root = m_dir.path();
        const QString config = root + QStringLiteral("/config");
        const QString games = root + QStringLiteral("/games");
        QDir().mkpath(config);
        QDir().mkpath(games);
        nexora::Logger::init(root + QStringLiteral("/logs"));

        const QStringList titles = {QStringLiteral("Alpha Quest"), QStringLiteral("Beta Racer"),
                                    QStringLiteral("Gamma Strike"), QStringLiteral("Delta Drift"),
                                    QStringLiteral("An Extremely Long Game Title That Should Never Break The Card Layout At All")};
        for (const QString &title : titles)
            writeFile(games + QLatin1Char('/') + title + QStringLiteral(".mock"), "test");

        QJsonObject mock{{"id", "mock"}, {"name", "Mock Emulator"},
                         {"executable", QStringLiteral(NEXORA_MOCK_EMULATOR)},
                         {"arguments", QJsonArray{"--sleep-ms", "300", "{game}"}},
                         {"platforms", QJsonArray{"Test Platform"}},
                         {"extensions", QJsonArray{"mock"}}};
        QJsonObject missing{{"id", "missing"}, {"name", "Missing Emulator"},
                            {"executable", "/nonexistent/emulator"}, {"extensions", QJsonArray{"none"}}};
        writeFile(config + QStringLiteral("/emulators.json"),
                  QJsonDocument(QJsonObject{{"version", 1}, {"emulators", QJsonArray{mock, missing}}}).toJson());
        writeFile(config + QStringLiteral("/settings.json"),
                  QJsonDocument(QJsonObject{{"firstRunCompleted", true}, {"fullscreen", false},
                                            {"reduceMotion", true},
                                            {"libraryDirectories", QJsonArray{games}}}).toJson());
        m_backend = new nexora::Backend(config, this);
        m_backend->registerQmlTypes();
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // Qt 6.4 does not add the default resource import path for QuickTest.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        engine->rootContext()->setContextProperty(QStringLiteral("testSandboxPath"), m_dir.path());
        m_backend->library()->refresh();
    }

private:
    QTemporaryDir m_dir;
    nexora::Backend *m_backend = nullptr;
};

QUICK_TEST_MAIN_WITH_SETUP(nexora_ui, Setup)

#include "main.moc"
