// Discovers and runs every *.qml file in this directory with QtQuickTest. tst_theme.qml
// needs nothing but the Nexora.Console module (Theme is a plain singleton); the other files
// load real screens/components against a real (sandboxed) backend, the same way
// tests/integration/tst_qmlsmoke.cpp drives the whole Main.qml, just one piece at a time.
//
// The sandbox has one pausable fake emulator ("fake", sleeps 30 s, handles *.rom) and a
// library folder with two games, "Alpha Quest" and "Beta Racer", scanned before QML starts.
#include "UiContext.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

namespace {

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(content);
}

} // namespace

class Setup : public QObject
{
    Q_OBJECT
public slots:
    // QuickTest calls this once QCoreApplication exists, before the QML engine starts.
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        const QString emulators = m_dir.filePath(QStringLiteral("emulators"));
        const QString games = m_dir.filePath(QStringLiteral("games"));
        writeFile(emulators + QStringLiteral("/fake.json"),
                  "{\"id\":\"fake\",\"name\":\"Fake\",\"executable\":\"" NEXORA_FAKE_EMULATOR "\","
                  "\"arguments\":[\"--sleep-ms\",\"30000\",\"{file}\"],\"extensions\":[\".rom\"],"
                  "\"pausable\":true}");
        writeFile(games + QStringLiteral("/Alpha Quest.rom"), "rom");
        writeFile(games + QStringLiteral("/Beta Racer.rom"), "rom");

        nexora::ui::UiContext::Options options;
        options.app = {emulators, QStringLiteral("test"), {}, false, {}};
        options.settings = {{}, m_dir.filePath(QStringLiteral("settings.json"))};
        options.layout = nexora::shared::DataLayout::underRoot(m_dir.filePath(QStringLiteral("state")));
        options.databasePath = m_dir.filePath(QStringLiteral("library.sqlite"));
        options.platformOverride = QStringLiteral("desktop");
        m_context = std::make_unique<nexora::ui::UiContext>(options);
        m_context->start();
        m_context->library()->addDirectory(games);
        m_context->gameLibrary().waitForScan();
        QCoreApplication::processEvents();
    }

    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // Qt 6.4 does not add the default resource import path for QuickTest.
        engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
        engine->rootContext()->setContextProperty(QStringLiteral("testApp"), m_context->app());
        engine->rootContext()->setContextProperty(QStringLiteral("testLibrary"), m_context->library());
        engine->rootContext()->setContextProperty(QStringLiteral("testSettings"), m_context->settings());
        engine->rootContext()->setContextProperty(QStringLiteral("testSystem"), m_context->system());
        engine->rootContext()->setContextProperty(QStringLiteral("testUpdates"), m_context->updates());
        engine->rootContext()->setContextProperty(QStringLiteral("testGameBar"), m_context->gameBar());
    }

    void cleanupTestCase()
    {
        if (m_context)
            m_context->shutdown();
    }

private:
    QTemporaryDir m_dir;
    std::unique_ptr<nexora::ui::UiContext> m_context;
};

QUICK_TEST_MAIN_WITH_SETUP(nexora_console_qml, Setup)

#include "main.moc"
