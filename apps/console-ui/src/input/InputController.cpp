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
#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>

namespace nexora::input {

namespace {

// Names every button, including the directions (ButtonMapping only names the remappable,
// non-directional ones).
QString rawButtonName(GamepadButton button)
{
    switch (button) {
    case GamepadButton::South: return QStringLiteral("south");
    case GamepadButton::East: return QStringLiteral("east");
    case GamepadButton::West: return QStringLiteral("west");
    case GamepadButton::North: return QStringLiteral("north");
    case GamepadButton::Start: return QStringLiteral("start");
    case GamepadButton::Select: return QStringLiteral("select");
    case GamepadButton::Guide: return QStringLiteral("guide");
    case GamepadButton::DpadUp: return QStringLiteral("dpadUp");
    case GamepadButton::DpadDown: return QStringLiteral("dpadDown");
    case GamepadButton::DpadLeft: return QStringLiteral("dpadLeft");
    case GamepadButton::DpadRight: return QStringLiteral("dpadRight");
    case GamepadButton::L1: return QStringLiteral("l1");
    case GamepadButton::R1: return QStringLiteral("r1");
    }
    return QStringLiteral("button");
}

QString rawAxisName(GamepadAxis axis)
{
    switch (axis) {
    case GamepadAxis::LeftX: return QStringLiteral("LeftX");
    case GamepadAxis::LeftY: return QStringLiteral("LeftY");
    case GamepadAxis::HatX: return QStringLiteral("HatX");
    case GamepadAxis::HatY: return QStringLiteral("HatY");
    }
    return QStringLiteral("axis");
}

} // namespace

InputController::InputController(QObject *parent)
    : QObject(parent)
    , m_navigator(new GamepadNavigator(this))
    , m_targetProvider([] { return QGuiApplication::focusWindow(); })
    , m_statusOverride(QStringLiteral("Controller input not started"))
{
    m_testExitTimer.setSingleShot(true);
    connect(&m_testExitTimer, &QTimer::timeout, this, [this] {
        if (!m_testMode)
            return;
        setTestMode(false);
        qCInfo(lcInput) << "Controller test mode left (Back held)";
        emit testModeExited();
    });
    connect(m_navigator, &GamepadNavigator::actionTriggered, this, [this](NavAction action) {
        if (m_testMode)
            return; // reported through rawEvent() instead
        const bool directional = action == NavAction::Up || action == NavAction::Down || action == NavAction::Left
            || action == NavAction::Right;
        if (m_capturing && !directional)
            return; // reported through buttonCaptured() instead
        injectAction(action);
    });
    connect(m_navigator, &GamepadNavigator::buttonPressed, this, [this](GamepadButton button, quint32) {
        if (m_capturing && !m_testMode && !m_suspended) {
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
    connect(m_navigator, &GamepadNavigator::rawButtonEvent, this, [this](GamepadButton button, bool pressed, quint32) {
        if (!m_testMode)
            return;
        if (button == backButton()) {
            if (!pressed)
                m_testExitTimer.stop();
            else if (!m_testExitTimer.isActive())
                m_testExitTimer.start(m_testExitHold);
        }
        report(QStringLiteral("%1 %2").arg(rawButtonName(button),
                                           pressed ? QStringLiteral("pressed") : QStringLiteral("released")));
    });
    connect(m_navigator, &GamepadNavigator::rawAxisEvent, this, [this](GamepadAxis axis, double value, quint32) {
        if (!m_testMode)
            return;
        report(QStringLiteral("%1 %2").arg(rawAxisName(axis)).arg(value, 0, 'f', 2));
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
    // Devices first: whoever listens to statusChanged must see an up-to-date device list.
    connect(backend.get(), &InputBackend::statusChanged, this, &InputController::refreshDevices);
    connect(backend.get(), &InputBackend::statusChanged, this, &InputController::statusChanged);
    if (!backend->start()) {
        qCInfo(lcInput).noquote() << backend->name() << "backend failed:" << backend->statusText();
        return false;
    }
    m_backend = backend.release();
    m_backend->setParent(this);
    qCInfo(lcInput).noquote() << "Controller input backend:" << m_backend->name() << '-' << m_backend->statusText();
    emit statusChanged();
    refreshDevices(); // pads already connected when the backend started
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
    refreshDevices(); // the backend is gone, so every device it reported is now gone too
}

QString InputController::backendName() const
{
    return m_backend ? m_backend->name() : QStringLiteral("none");
}

QString InputController::statusText() const
{
    return m_backend ? m_backend->statusText() : m_statusOverride;
}

void InputController::refreshDevices()
{
    const QList<InputBackend::DeviceInfo> current
        = m_backend ? m_backend->devices() : QList<InputBackend::DeviceInfo>{};
    const QString backend = backendName();
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);

    QVariantList kept;
    QStringList connected;
    for (const InputBackend::DeviceInfo &device : current) {
        const auto known = std::find_if(m_devices.cbegin(), m_devices.cend(), [&](const QVariant &entry) {
            return entry.toMap().value(QStringLiteral("id")).toString() == device.id;
        });
        if (known != m_devices.cend()) {
            kept.append(*known); // keeps the original connectedAt
            continue;
        }
        kept.append(QVariantMap{{QStringLiteral("id"), device.id},
                                {QStringLiteral("name"), device.name},
                                {QStringLiteral("backend"), backend},
                                {QStringLiteral("connectedAt"), now}});
        connected.append(device.name);
    }

    QStringList disconnected;
    for (const QVariant &entry : m_devices) {
        const QVariantMap map = entry.toMap();
        const QString id = map.value(QStringLiteral("id")).toString();
        const bool stillThere = std::any_of(current.cbegin(), current.cend(),
                                            [&](const InputBackend::DeviceInfo &d) { return d.id == id; });
        if (!stillThere)
            disconnected.append(map.value(QStringLiteral("name")).toString());
    }

    if (connected.isEmpty() && disconnected.isEmpty())
        return;
    m_devices = kept;
    emit devicesChanged();
    for (const QString &name : disconnected) {
        qCInfo(lcInput).noquote() << "Controller disconnected:" << name;
        emit deviceDisconnected(name);
    }
    for (const QString &name : connected) {
        qCInfo(lcInput).noquote() << "Controller connected:" << name;
        emit deviceConnected(name);
    }
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

void InputController::setTestMode(bool enabled)
{
    if (m_testMode == enabled)
        return;
    m_testMode = enabled;
    m_testExitTimer.stop();
    m_navigator->reset(); // no held state or auto-repeat carries across the switch
    qCInfo(lcInput) << "Controller test mode" << (enabled ? "entered" : "left");
    emit testModeChanged();
}

void InputController::setTestModeExitHold(std::chrono::milliseconds duration)
{
    m_testExitHold = std::max(duration, std::chrono::milliseconds{1});
}

GamepadButton InputController::backButton() const
{
    // Follows a remapped Back, so the way out of test mode is always the UI's Back button.
    return m_navigator->mapping().buttonFor(NavAction::Back).value_or(GamepadButton::East);
}

void InputController::report(const QString &text)
{
    m_lastRawEvent = text;
    qCDebug(lcInput).noquote() << "Test mode:" << text;
    emit rawEvent(text);
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
