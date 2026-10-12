// Discovers and runs every *.qml file in this directory with QtQuickTest. tst_theme.qml
// needs nothing but the Nexora.Console module (Theme is a plain singleton); tst_settings_ui
// .qml loads the real SettingsScreen against a real (sandboxed) backend, the same way
// tests/integration/tst_qmlsmoke.cpp drives the whole Main.qml, just for one screen.
#include "UiContext.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(Nexora_ConsolePlugin)

class Setup : public QObject
{
    Q_OBJECT
public slots:
    // QuickTest calls this once QCoreApplication exists, before the QML engine starts.
    void applicationAvailable()
    {
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        nexora::ui::UiContext::Options options;
        options.app = {m_dir.path(), QStringLiteral("test"), {}, false, {}};
        options.settings = {{}, m_dir.filePath(QStringLiteral("settings.json"))};
        options.databasePath = m_dir.filePath(QStringLiteral("library.sqlite"));
        options.platformOverride = QStringLiteral("desktop");
        m_context = std::make_unique<nexora::ui::UiContext>(options);
        m_context->start();
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
    }

private:
    QTemporaryDir m_dir;
    std::unique_ptr<nexora::ui::UiContext> m_context;
};

QUICK_TEST_MAIN_WITH_SETUP(nexora_console_qml, Setup)

#include "main.moc"
