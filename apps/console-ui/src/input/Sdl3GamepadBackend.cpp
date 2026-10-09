#include "input/Sdl3GamepadBackend.h"

#include "input/GamepadNavigator.h"
#include "nexora/shared/Logging.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <optional>

namespace nexora::input {

namespace {

constexpr int kPumpIntervalMs = 16;

std::optional<GamepadButton> sdlButtonToButton(Uint8 button)
{
    switch (static_cast<SDL_GamepadButton>(button)) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return GamepadButton::South;
    case SDL_GAMEPAD_BUTTON_EAST: return GamepadButton::East;
    case SDL_GAMEPAD_BUTTON_WEST: return GamepadButton::West;
    case SDL_GAMEPAD_BUTTON_NORTH: return GamepadButton::North;
    case SDL_GAMEPAD_BUTTON_START: return GamepadButton::Start;
    case SDL_GAMEPAD_BUTTON_BACK: return GamepadButton::Select;
    case SDL_GAMEPAD_BUTTON_GUIDE: return GamepadButton::Guide;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return GamepadButton::DpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return GamepadButton::DpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return GamepadButton::DpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButton::DpadRight;
    default: return std::nullopt;
    }
}

} // namespace

Sdl3GamepadBackend::Sdl3GamepadBackend(GamepadNavigator *navigator, QObject *parent)
    : InputBackend(navigator, parent)
{
    m_pumpTimer.setInterval(kPumpIntervalMs);
    connect(&m_pumpTimer, &QTimer::timeout, this, &Sdl3GamepadBackend::pump);
}

Sdl3GamepadBackend::~Sdl3GamepadBackend()
{
    m_pumpTimer.stop();
    if (!m_initialized)
        return;
    for (auto &[id, pad] : m_pads) {
        if (pad.gamepad)
            SDL_CloseGamepad(pad.gamepad);
    }
    m_pads.clear();
    SDL_Quit();
}

bool Sdl3GamepadBackend::start()
{
    if (m_initialized)
        return true;

    // Keep receiving input while another window (e.g. a running emulator) has focus, and
    // leave SIGINT/SIGTERM handling to the Qt application.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        m_status = QStringLiteral("SDL3 gamepad init failed: %1").arg(QString::fromUtf8(SDL_GetError()));
        qCWarning(lcInput).noquote() << m_status;
        emit statusChanged();
        return false;
    }
    m_initialized = true;
    qCInfo(lcInput) << "SDL3 gamepad backend started, SDL" << SDL_VERSIONNUM_MAJOR(SDL_GetVersion())
                    << SDL_VERSIONNUM_MINOR(SDL_GetVersion()) << SDL_VERSIONNUM_MICRO(SDL_GetVersion());
    updateStatus();
    pump(); // already-connected pads arrive as SDL_EVENT_GAMEPAD_ADDED
    m_pumpTimer.start();
    return true;
}

QString Sdl3GamepadBackend::name() const
{
    return QStringLiteral("sdl3");
}

QString Sdl3GamepadBackend::statusText() const
{
    return m_status;
}

QStringList Sdl3GamepadBackend::deviceNames() const
{
    QStringList names;
    for (const auto &[id, pad] : m_pads)
        names.append(pad.name);
    return names;
}

void Sdl3GamepadBackend::pump()
{
    bool devicesChanged = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            const SDL_JoystickID id = event.gdevice.which;
            if (m_pads.count(id))
                break;
            SDL_Gamepad *gamepad = SDL_OpenGamepad(id);
            if (!gamepad) {
                qCWarning(lcInput).noquote() << "SDL_OpenGamepad failed:" << SDL_GetError();
                break;
            }
            const char *rawName = SDL_GetGamepadName(gamepad);
            Pad pad{gamepad, rawName ? QString::fromUtf8(rawName) : QStringLiteral("Gamepad %1").arg(id)};
            qCInfo(lcInput).noquote() << "Gamepad connected:" << pad.name;
            m_pads.emplace(id, std::move(pad));
            devicesChanged = true;
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            const auto it = m_pads.find(event.gdevice.which);
            if (it == m_pads.end())
                break;
            qCInfo(lcInput).noquote() << "Gamepad disconnected:" << it->second.name;
            SDL_CloseGamepad(it->second.gamepad);
            m_pads.erase(it);
            navigator()->reset();
            devicesChanged = true;
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            if (const auto button = sdlButtonToButton(event.gbutton.button))
                navigator()->buttonEvent(*button, event.gbutton.down);
            break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            const double value = std::clamp(event.gaxis.value / 32767.0, -1.0, 1.0);
            if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
                navigator()->axisEvent(GamepadAxis::LeftX, value);
            else if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
                navigator()->axisEvent(GamepadAxis::LeftY, value);
            break;
        }
        default:
            break;
        }
    }
    if (devicesChanged)
        updateStatus();
}

void Sdl3GamepadBackend::updateStatus()
{
    const auto count = m_pads.size();
    m_status = count == 0 ? QStringLiteral("No gamepad detected; waiting for one to be connected")
             : count == 1 ? QStringLiteral("1 gamepad connected")
                          : QStringLiteral("%1 gamepads connected").arg(count);
    emit statusChanged();
}

} // namespace nexora::input
