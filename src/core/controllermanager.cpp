#include "controllermanager.h"

#include <QCoreApplication>
#include <QKeyEvent>

#include <algorithm>
#include <cmath>

#ifdef NEXORA_HAVE_SDL
#include <SDL.h>
#endif

namespace nexora {

namespace {

constexpr qreal StickPressThreshold = 0.5;
constexpr qreal StickReleaseThreshold = 0.35; // hysteresis

struct ActionInfo
{
    const char *action;
    int key;
    const char *label;
    const char *controllerInput;
    const char *keyboardInput;
};

const ActionInfo Actions[] = {
    {"confirm", Qt::Key_Return, "Confirm", "South button (A / Cross)", "Enter / Space"},
    {"back", Qt::Key_Escape, "Back", "East button (B / Circle)", "Esc"},
    {"secondary", Qt::Key_F, "Secondary action", "West button (X / Square)", "F"},
    {"search", Qt::Key_Slash, "Search", "North button (Y / Triangle)", "/"},
    {"options", Qt::Key_Menu, "Options", "Start / Menu button", "M / Menu"},
    {"prevTab", Qt::Key_PageUp, "Previous tab", "Left shoulder (LB / L1)", "Page Up"},
    {"nextTab", Qt::Key_PageDown, "Next tab", "Right shoulder (RB / R1)", "Page Down"},
    {"up", Qt::Key_Up, "Up", "D-pad up / left stick", "Arrow Up"},
    {"down", Qt::Key_Down, "Down", "D-pad down / left stick", "Arrow Down"},
    {"left", Qt::Key_Left, "Left", "D-pad left / left stick", "Arrow Left"},
    {"right", Qt::Key_Right, "Right", "D-pad right / left stick", "Arrow Right"},
};

int keyForAction(const QString &action)
{
    for (const ActionInfo &info : Actions) {
        if (action == QLatin1String(info.action))
            return info.key;
    }
    return 0;
}

QString textForKey(int key)
{
    switch (key) {
    case Qt::Key_Return: return QStringLiteral("\r");
    case Qt::Key_Escape: return QStringLiteral("\x1b");
    case Qt::Key_F: return QStringLiteral("f");
    case Qt::Key_Slash: return QStringLiteral("/");
    default: return {};
    }
}

#ifdef NEXORA_HAVE_SDL
SDL_GameController *asController(void *handle)
{
    return static_cast<SDL_GameController *>(handle);
}

QString controllerTypeName(SDL_GameController *controller)
{
    switch (SDL_GameControllerGetType(controller)) {
    case SDL_CONTROLLER_TYPE_XBOX360:
    case SDL_CONTROLLER_TYPE_XBOXONE:
        return QStringLiteral("xbox");
    case SDL_CONTROLLER_TYPE_PS3:
    case SDL_CONTROLLER_TYPE_PS4:
    case SDL_CONTROLLER_TYPE_PS5:
        return QStringLiteral("playstation");
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
        return QStringLiteral("nintendo");
    default:
        return QStringLiteral("generic");
    }
}

QString buttonName(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: return QStringLiteral("south");
    case SDL_CONTROLLER_BUTTON_B: return QStringLiteral("east");
    case SDL_CONTROLLER_BUTTON_X: return QStringLiteral("west");
    case SDL_CONTROLLER_BUTTON_Y: return QStringLiteral("north");
    case SDL_CONTROLLER_BUTTON_START: return QStringLiteral("start");
    case SDL_CONTROLLER_BUTTON_BACK: return QStringLiteral("back");
    case SDL_CONTROLLER_BUTTON_GUIDE: return QStringLiteral("guide");
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return QStringLiteral("leftshoulder");
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return QStringLiteral("rightshoulder");
    case SDL_CONTROLLER_BUTTON_LEFTSTICK: return QStringLiteral("leftstick");
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return QStringLiteral("rightstick");
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return QStringLiteral("dpup");
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return QStringLiteral("dpdown");
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return QStringLiteral("dpleft");
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return QStringLiteral("dpright");
    default: return {};
    }
}

QString axisName(int axis)
{
    switch (axis) {
    case SDL_CONTROLLER_AXIS_LEFTX: return QStringLiteral("leftx");
    case SDL_CONTROLLER_AXIS_LEFTY: return QStringLiteral("lefty");
    case SDL_CONTROLLER_AXIS_RIGHTX: return QStringLiteral("rightx");
    case SDL_CONTROLLER_AXIS_RIGHTY: return QStringLiteral("righty");
    case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return QStringLiteral("lefttrigger");
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return QStringLiteral("righttrigger");
    default: return {};
    }
}

int keyForButton(int button)
{
    switch (button) {
    case SDL_CONTROLLER_BUTTON_A: return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B: return Qt::Key_Escape;
    case SDL_CONTROLLER_BUTTON_X: return Qt::Key_F;
    case SDL_CONTROLLER_BUTTON_Y: return Qt::Key_Slash;
    case SDL_CONTROLLER_BUTTON_START: return Qt::Key_Menu;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Qt::Key_PageUp;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Qt::Key_PageDown;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Qt::Key_Right;
    default: return 0;
    }
}

bool isDirectional(int key)
{
    return key == Qt::Key_Up || key == Qt::Key_Down || key == Qt::Key_Left || key == Qt::Key_Right;
}
#endif

} // namespace

ControllerManager::ControllerManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_repeatTimer, &QTimer::timeout, this, [this] {
        if (!m_repeatKey)
            return;
        postKey(m_repeatKey);
        if (m_repeatTimer.interval() != m_repeatIntervalMs)
            m_repeatTimer.setInterval(m_repeatIntervalMs);
    });

#ifdef NEXORA_HAVE_SDL
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        m_unavailableReason = QStringLiteral("Game controller support could not be initialised: %1")
                                  .arg(QString::fromUtf8(SDL_GetError()));
        qWarning("Controllers: %s", qPrintable(m_unavailableReason));
        return;
    }
    m_available = true;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        openController(i);
    m_pollTimer.setInterval(16);
    connect(&m_pollTimer, &QTimer::timeout, this, &ControllerManager::poll);
    m_pollTimer.start();
    qInfo("Controllers: SDL initialised, %d controller(s) connected", count());
#else
    m_unavailableReason = QStringLiteral("This build was compiled without controller support (SDL2).");
#endif
}

ControllerManager::~ControllerManager()
{
    m_pollTimer.stop();
    m_repeatTimer.stop();
#ifdef NEXORA_HAVE_SDL
    for (void *handle : std::as_const(m_handles))
        SDL_GameControllerClose(asController(handle));
    m_handles.clear();
    if (m_available)
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}

bool ControllerManager::available() const { return m_available; }
QString ControllerManager::unavailableReason() const { return m_unavailableReason; }
QVariantList ControllerManager::controllers() const { return m_controllers; }
int ControllerManager::count() const { return int(m_controllers.size()); }
bool ControllerManager::suppressed() const { return m_suppressed; }
QString ControllerManager::lastInputDevice() const { return m_lastInput; }
QString ControllerManager::controllerType() const { return m_controllerType; }
int ControllerManager::repeatDelayMs() const { return m_repeatDelayMs; }
int ControllerManager::repeatIntervalMs() const { return m_repeatIntervalMs; }

void ControllerManager::setSuppressed(bool value)
{
    if (m_suppressed == value)
        return;
    m_suppressed = value;
    if (value) {
        stopRepeat();
        m_stickDirection = 0;
    }
    emit suppressedChanged();
}

void ControllerManager::setRepeatDelayMs(int ms)
{
    ms = qMax(50, ms);
    if (m_repeatDelayMs == ms)
        return;
    m_repeatDelayMs = ms;
    emit repeatChanged();
}

void ControllerManager::setRepeatIntervalMs(int ms)
{
    ms = qMax(20, ms);
    if (m_repeatIntervalMs == ms)
        return;
    m_repeatIntervalMs = ms;
    emit repeatChanged();
}

QVariantList ControllerManager::mappings() const
{
    QVariantList list;
    for (const ActionInfo &info : Actions) {
        list.append(QVariantMap{{QStringLiteral("action"), QLatin1String(info.action)},
                                {QStringLiteral("label"), QLatin1String(info.label)},
                                {QStringLiteral("controllerInput"), QLatin1String(info.controllerInput)},
                                {QStringLiteral("keyboardInput"), QLatin1String(info.keyboardInput)}});
    }
    return list;
}

void ControllerManager::setTargetWindow(QWindow *window)
{
    m_window = window;
}

void ControllerManager::noteKeyboardInput()
{
    setLastInputDevice(QStringLiteral("keyboard"));
}

QString ControllerManager::buttonLabel(const QString &action) const
{
    static const QStringList order{QStringLiteral("confirm"), QStringLiteral("back"), QStringLiteral("secondary"),
                                   QStringLiteral("search"), QStringLiteral("options"), QStringLiteral("prevTab"),
                                   QStringLiteral("nextTab")};
    const qsizetype i = order.indexOf(action);
    if (i < 0)
        return {};
    static const QStringList keyboard{QStringLiteral("Enter"), QStringLiteral("Esc"), QStringLiteral("F"),
                                      QStringLiteral("/"), QStringLiteral("M"), QStringLiteral("PgUp"),
                                      QStringLiteral("PgDn")};
    static const QStringList xbox{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("X"), QStringLiteral("Y"),
                                  QStringLiteral("Menu"), QStringLiteral("LB"), QStringLiteral("RB")};
    static const QStringList playstation{QStringLiteral("Cross"), QStringLiteral("Circle"), QStringLiteral("Square"),
                                         QStringLiteral("Triangle"), QStringLiteral("Options"), QStringLiteral("L1"),
                                         QStringLiteral("R1")};
    // Positional mapping (SDL button labels hint is off), so south is labelled B.
    static const QStringList nintendo{QStringLiteral("B"), QStringLiteral("A"), QStringLiteral("Y"),
                                      QStringLiteral("X"), QStringLiteral("+"), QStringLiteral("L"),
                                      QStringLiteral("R")};
    static const QStringList generic{QStringLiteral("South"), QStringLiteral("East"), QStringLiteral("West"),
                                     QStringLiteral("North"), QStringLiteral("Start"), QStringLiteral("L1"),
                                     QStringLiteral("R1")};
    if (m_lastInput != QLatin1String("controller"))
        return keyboard.at(i);
    if (m_controllerType == QLatin1String("xbox"))
        return xbox.at(i);
    if (m_controllerType == QLatin1String("playstation"))
        return playstation.at(i);
    if (m_controllerType == QLatin1String("nintendo"))
        return nintendo.at(i);
    return generic.at(i);
}

void ControllerManager::injectAction(const QString &action)
{
    const int key = keyForAction(action);
    if (!key) {
        qWarning("Controllers: unknown action \"%s\"", qPrintable(action));
        return;
    }
    setLastInputDevice(QStringLiteral("controller"));
    postKey(key);
}

void ControllerManager::postKey(int qtKey)
{
    if (m_suppressed || !m_window)
        return;
    const QString text = textForKey(qtKey);
    QCoreApplication::postEvent(m_window, new QKeyEvent(QEvent::KeyPress, qtKey, Qt::NoModifier, 0, 0,
                                                        SyntheticMarker, text));
    QCoreApplication::postEvent(m_window, new QKeyEvent(QEvent::KeyRelease, qtKey, Qt::NoModifier, 0, 0,
                                                        SyntheticMarker, text));
}

void ControllerManager::setLastInputDevice(const QString &device, const QString &type)
{
    const QString newType = type.isEmpty() ? m_controllerType : type;
    if (m_lastInput == device && m_controllerType == newType)
        return;
    m_lastInput = device;
    m_controllerType = newType;
    emit lastInputDeviceChanged();
}

void ControllerManager::startRepeat(int qtKey)
{
    if (m_suppressed)
        return;
    postKey(qtKey);
    m_repeatKey = qtKey;
    m_repeatTimer.start(m_repeatDelayMs);
}

void ControllerManager::stopRepeat()
{
    m_repeatKey = 0;
    m_repeatTimer.stop();
}

void ControllerManager::poll()
{
#ifdef NEXORA_HAVE_SDL
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED:
            openController(event.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            closeController(event.cdevice.which);
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            handleButton(event.cbutton.which, event.cbutton.button, event.type == SDL_CONTROLLERBUTTONDOWN);
            break;
        case SDL_CONTROLLERAXISMOTION:
            handleAxis(event.caxis.which, event.caxis.axis, qBound(-1.0, event.caxis.value / 32767.0, 1.0));
            break;
        default:
            break;
        }
    }
#endif
}

void ControllerManager::openController(int deviceIndex)
{
#ifdef NEXORA_HAVE_SDL
    if (!SDL_IsGameController(deviceIndex))
        return;
    const int instanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
    if (instanceId < 0 || m_handles.contains(instanceId))
        return; // SDL also reports devices present at startup as "added"
    SDL_GameController *controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        qWarning("Controllers: cannot open device %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    m_handles.insert(instanceId, controller);
    const QString name = QString::fromUtf8(SDL_GameControllerName(controller)).trimmed();
    const QString displayName = name.isEmpty() ? QStringLiteral("Game controller") : name;
    m_controllers.append(QVariantMap{{QStringLiteral("instanceId"), instanceId},
                                     {QStringLiteral("name"), displayName},
                                     {QStringLiteral("type"), controllerTypeName(controller)}});
    qInfo("Controllers: connected \"%s\" (%s)", qPrintable(displayName), qPrintable(controllerTypeName(controller)));
    emit controllersChanged();
    emit controllerConnected(displayName);
#else
    Q_UNUSED(deviceIndex)
#endif
}

void ControllerManager::closeController(int instanceId)
{
#ifdef NEXORA_HAVE_SDL
    void *handle = m_handles.take(instanceId);
    if (!handle)
        return;
    SDL_GameControllerClose(asController(handle));
    QString name;
    for (qsizetype i = 0; i < m_controllers.size(); ++i) {
        const QVariantMap entry = m_controllers.at(i).toMap();
        if (entry.value(QStringLiteral("instanceId")).toInt() == instanceId) {
            name = entry.value(QStringLiteral("name")).toString();
            m_controllers.removeAt(i);
            break;
        }
    }
    stopRepeat();
    m_stickDirection = 0;
    qInfo("Controllers: disconnected \"%s\"", qPrintable(name));
    emit controllersChanged();
    emit controllerDisconnected(name);
#else
    Q_UNUSED(instanceId)
#endif
}

QString ControllerManager::typeOf(int instanceId) const
{
    for (const QVariant &v : m_controllers) {
        const QVariantMap entry = v.toMap();
        if (entry.value(QStringLiteral("instanceId")).toInt() == instanceId)
            return entry.value(QStringLiteral("type")).toString();
    }
    return QStringLiteral("generic");
}

void ControllerManager::handleButton(int instanceId, int button, bool pressed)
{
#ifdef NEXORA_HAVE_SDL
    const QString name = buttonName(button);
    if (!name.isEmpty())
        emit buttonEvent(name, pressed);
    const int key = keyForButton(button);
    if (!key)
        return;
    if (!pressed) {
        if (isDirectional(key) && m_repeatKey == key)
            stopRepeat();
        return;
    }
    setLastInputDevice(QStringLiteral("controller"), typeOf(instanceId));
    if (isDirectional(key))
        startRepeat(key);
    else
        postKey(key);
#else
    Q_UNUSED(instanceId)
    Q_UNUSED(button)
    Q_UNUSED(pressed)
#endif
}

void ControllerManager::handleAxis(int instanceId, int axis, qreal value)
{
#ifdef NEXORA_HAVE_SDL
    const QString name = axisName(axis);
    if (!name.isEmpty())
        emit axisEvent(name, value);
    if (axis != SDL_CONTROLLER_AXIS_LEFTX && axis != SDL_CONTROLLER_AXIS_LEFTY)
        return;
    SDL_GameController *controller = asController(m_handles.value(instanceId));
    if (!controller)
        return;
    const qreal x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0;
    const qreal y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0;

    int candidate = 0;
    if (std::max(std::abs(x), std::abs(y)) >= StickPressThreshold) {
        if (std::abs(x) > std::abs(y))
            candidate = x < 0 ? Qt::Key_Left : Qt::Key_Right;
        else
            candidate = y < 0 ? Qt::Key_Up : Qt::Key_Down;
    }
    if (candidate == m_stickDirection)
        return;
    if (m_stickDirection && !candidate) {
        // Keep the held direction until the stick falls below the release threshold.
        const qreal held = (m_stickDirection == Qt::Key_Left) ? -x
                           : (m_stickDirection == Qt::Key_Right) ? x
                           : (m_stickDirection == Qt::Key_Up) ? -y
                                                               : y;
        if (held >= StickReleaseThreshold)
            return;
    }
    if (m_stickDirection && m_repeatKey == m_stickDirection)
        stopRepeat();
    m_stickDirection = candidate;
    if (candidate) {
        setLastInputDevice(QStringLiteral("controller"), typeOf(instanceId));
        startRepeat(candidate);
    }
#else
    Q_UNUSED(instanceId)
    Q_UNUSED(axis)
    Q_UNUSED(value)
#endif
}

} // namespace nexora
