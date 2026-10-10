#include "backend.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QWindow>

#include "appsettings.h"
#include "controllermanager.h"
#include "diagnostics.h"
#include "directorybrowser.h"
#include "emulatormanager.h"
#include "emulatorregistry.h"
#include "gamefiltermodel.h"
#include "gamelibrary.h"

namespace nexora {

Backend::Backend(const QString &configDirectory, QObject *parent)
    : QObject(parent)
    , m_settings(new AppSettings(configDirectory, this))
    , m_emulators(new EmulatorRegistry(configDirectory, this))
    , m_launcher(new EmulatorManager(m_emulators, this))
    , m_library(new GameLibrary(configDirectory, m_settings, m_emulators, this))
    , m_controllers(new ControllerManager(this))
    , m_diagnostics(new Diagnostics(configDirectory, m_emulators, m_launcher, m_controllers, m_library, this))
{
    // Recent games follow the real process lifecycle: an entry is recorded
    // only once the emulator process has actually started.
    connect(m_launcher, &EmulatorManager::processStarted, this,
            [this](const QString &, const QString &gamePath) {
                if (!gamePath.isEmpty())
                    m_library->recordLaunch(gamePath);
            });
    // The emulator owns the screen while running: do not drive the hidden UI.
    connect(m_launcher, &EmulatorManager::stateChanged, this, [this] {
        m_controllers->setSuppressed(m_launcher->state() == EmulatorManager::Running);
    });
    // Bring the console UI back when the external process ends.
    connect(m_launcher, &EmulatorManager::processFinished, this, [this] {
        if (!m_window)
            return;
        m_window->raise();
        m_window->requestActivate();
    });
    connect(m_settings, &AppSettings::libraryDirectoriesChanged, m_library, &GameLibrary::refresh);
    connect(m_emulators, &EmulatorRegistry::reloaded, m_library, &GameLibrary::refresh);
}

Backend::~Backend()
{
    if (qGuiApp)
        qGuiApp->removeEventFilter(this);
}

void Backend::registerQmlTypes()
{
    const char *uri = "Nexora.Core";
    qmlRegisterSingletonInstance(uri, 1, 0, "Settings", m_settings);
    qmlRegisterSingletonInstance(uri, 1, 0, "Emulators", m_emulators);
    qmlRegisterSingletonInstance(uri, 1, 0, "Launcher", m_launcher);
    qmlRegisterSingletonInstance(uri, 1, 0, "Library", m_library);
    qmlRegisterSingletonInstance(uri, 1, 0, "Controllers", m_controllers);
    qmlRegisterSingletonInstance(uri, 1, 0, "Diagnostics", m_diagnostics);
    qmlRegisterType<GameFilterModel>(uri, 1, 0, "GameFilterModel");
    qmlRegisterType<DirectoryBrowser>(uri, 1, 0, "DirectoryBrowser");
    qmlRegisterUncreatableType<GameModel>(uri, 1, 0, "GameModel", QStringLiteral("Use Library.model"));
}

void Backend::attachWindow(QWindow *window)
{
    m_window = window;
    m_controllers->setTargetWindow(window);
    if (qGuiApp)
        qGuiApp->installEventFilter(this);
}

bool Backend::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->nativeModifiers() != ControllerManager::SyntheticMarker)
            m_controllers->noteKeyboardInput();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace nexora
