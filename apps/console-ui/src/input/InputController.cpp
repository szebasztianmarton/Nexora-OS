#include "input/InputController.h"

#include "input/GamepadNavigator.h"
#include "input/InputBackend.h"
#include "nexora/shared/Logging.h"

#ifdef NEXORA_HAVE_EVDEV
#include "input/EvdevGamepadBackend.h"
#endif
#ifdef NEXORA_HAVE_SDL3
#include "input/Sdl3GamepadBackend.h"
#endif

#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>

namespace nexora::input {

InputController::InputController(QObject *parent)
    : QObject(parent)
    , m_navigator(new GamepadNavigator(this))
    , m_targetProvider([] { return QGuiApplication::focusWindow(); })
    , m_statusOverride(QStringLiteral("Controller input not started"))
{
    connect(m_navigator, &GamepadNavigator::actionTriggered, this, [this](NavAction action) {
        const bool directional = action == NavAction::Up || action == NavAction::Down || action == NavAction::Left
            || action == NavAction::Right;
        if (m_capturing && !directional)
            return; // reported through buttonCaptured() instead
        injectAction(action);
    });
    connect(m_navigator, &GamepadNavigator::buttonPressed, this, [this](GamepadButton button, quint32) {
        if (m_capturing && !m_suspended) {
            // Queued: the action for this same press (emitted right after) must still be
            // swallowed by capture mode, not run with the freshly applied mapping.
            const QString name = ButtonMapping::buttonName(button);
            QMetaObject::invokeMethod(this, [this, name] { emit buttonCaptured(name); }, Qt::QueuedConnection);
        }
    });
    connect(m_navigator, &GamepadNavigator::returnToLauncherRequested, this, [this] {
        qCInfo(lcInput) << "Return-to-launcher chord (Select + Start) detected";
        emit returnToLauncherRequested();
    });
}

InputController::~InputController()
{
    stopBackend();
}

bool InputController::start(const QString &backend)
{
    stopBackend();
    const QString requested = backend.trimmed().toLower();

    auto finishNone = [this](const QString &status, bool ok) {
        m_statusOverride = status;
        qCInfo(lcInput).noquote() << "Controller input:" << status;
        emit statusChanged();
        return ok;
    };

    if (requested == QLatin1String("none"))
        return finishNone(QStringLiteral("Controller input disabled"), true);

    if (requested == QLatin1String("sdl3") || requested == QLatin1String("auto")) {
#ifdef NEXORA_HAVE_SDL3
        if (startBackend(std::make_unique<Sdl3GamepadBackend>(m_navigator)))
            return true;
        if (requested == QLatin1String("sdl3"))
            return finishNone(QStringLiteral("SDL3 gamepad backend failed to start"), false);
        qCInfo(lcInput) << "SDL3 backend unavailable, falling back to evdev";
#else
        if (requested == QLatin1String("sdl3"))
            return finishNone(QStringLiteral("SDL3 gamepad backend not built in"), false);
#endif
    }

    if (requested == QLatin1String("evdev") || requested == QLatin1String("auto")) {
#ifdef NEXORA_HAVE_EVDEV
        if (startBackend(std::make_unique<EvdevGamepadBackend>(m_navigator)))
            return true;
        if (requested == QLatin1String("evdev"))
            return finishNone(QStringLiteral("evdev gamepad backend failed to start"), false);
#else
        if (requested == QLatin1String("evdev"))
            return finishNone(QStringLiteral("evdev gamepad backend not built in"), false);
#endif
        return finishNone(QStringLiteral("No gamepad backend available; keyboard only"), true);
    }

    qCWarning(lcInput) << "Unknown input backend" << backend << "(expected auto|evdev|sdl3|none)";
    return finishNone(QStringLiteral("Unknown input backend '%1'").arg(backend), false);
}

bool InputController::start(std::unique_ptr<InputBackend> backend)
{
    stopBackend();
    return startBackend(std::move(backend));
}

bool InputController::startBackend(std::unique_ptr<InputBackend> backend)
{
    connect(backend.get(), &InputBackend::statusChanged, this, &InputController::statusChanged);
    if (!backend->start()) {
        qCInfo(lcInput).noquote() << backend->name() << "backend failed:" << backend->statusText();
        return false;
    }
    m_backend = backend.release();
    m_backend->setParent(this);
    qCInfo(lcInput).noquote() << "Controller input backend:" << m_backend->name() << '-' << m_backend->statusText();
    emit statusChanged();
    return true;
}

void InputController::stopBackend()
{
    if (!m_backend)
        return;
    disconnect(m_backend, nullptr, this, nullptr);
    delete m_backend;
    m_backend = nullptr;
    m_navigator->reset();
}

QString InputController::backendName() const
{
    return m_backend ? m_backend->name() : QStringLiteral("none");
}

QString InputController::statusText() const
{
    return m_backend ? m_backend->statusText() : m_statusOverride;
}

QStringList InputController::devices() const
{
    return m_backend ? m_backend->deviceNames() : QStringList();
}

void InputController::setSuspended(bool suspended)
{
    if (m_suspended == suspended)
        return;
    m_suspended = suspended;
    m_navigator->reset(); // no stale auto-repeat across suspend/resume
    qCDebug(lcInput) << "Controller navigation" << (suspended ? "suspended" : "resumed");
    emit suspendedChanged();
}

void InputController::setMapping(const ButtonMapping &mapping)
{
    m_navigator->setMapping(mapping);
}

ButtonMapping InputController::mapping() const
{
    return m_navigator->mapping();
}

void InputController::setCapturing(bool capturing)
{
    m_capturing = capturing;
}

void InputController::setTargetWindowProvider(std::function<QWindow *()> provider)
{
    m_targetProvider = std::move(provider);
}

void InputController::injectAction(NavAction action)
{
    if (m_suspended) {
        qCDebug(lcInput) << "Dropping" << action << "(suspended)";
        return;
    }
    deliver(action);
}

int InputController::keyForAction(NavAction action)
{
    switch (action) {
    case NavAction::Up: return Qt::Key_Up;
    case NavAction::Down: return Qt::Key_Down;
    case NavAction::Left: return Qt::Key_Left;
    case NavAction::Right: return Qt::Key_Right;
    case NavAction::Accept: return Qt::Key_Return;
    case NavAction::Back: return Qt::Key_Escape;
    case NavAction::Menu: return Qt::Key_Menu;
    case NavAction::Favorite: return Qt::Key_Favorites;
    case NavAction::Search: return Qt::Key_Search;
    case NavAction::PrevTab: return Qt::Key_PageUp;
    case NavAction::NextTab: return Qt::Key_PageDown;
    case NavAction::Options: return Qt::Key_Context1;
    }
    return Qt::Key_unknown;
}

void InputController::deliver(NavAction action)
{
    QWindow *target = m_targetProvider ? m_targetProvider() : nullptr;
    if (!target) {
        qCDebug(lcInput) << "Dropping" << action << "(no focused window)";
        return;
    }
    const int key = keyForAction(action);
    const QString text = action == NavAction::Accept ? QStringLiteral("\r")
        : action == NavAction::Back                  ? QStringLiteral("\x1b")
                                                     : QString();
    // Posted (not sent) so delivery never re-enters the caller, e.g. a backend read loop.
    QCoreApplication::postEvent(target, new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier, text));
    QCoreApplication::postEvent(target, new QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier, text));
    emit actionDelivered(action);
}

} // namespace nexora::input
