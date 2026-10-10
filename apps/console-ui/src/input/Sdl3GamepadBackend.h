#pragma once

#include "input/InputBackend.h"

#include <QTimer>

#include <map>

struct SDL_Gamepad;

namespace nexora::input {

// Gamepad input via SDL3's gamepad API (uses SDL's controller database, so button layout
// is normalized across many controllers). Only the gamepad subsystem is initialized; SDL
// events are pumped from a 16 ms timer on the GUI thread.
class Sdl3GamepadBackend : public InputBackend
{
    Q_OBJECT

public:
    explicit Sdl3GamepadBackend(GamepadNavigator *navigator, QObject *parent = nullptr);
    ~Sdl3GamepadBackend() override;

    bool start() override;
    QString name() const override;
    QString statusText() const override;
    QStringList deviceNames() const override;

private:
    struct Pad
    {
        SDL_Gamepad *gamepad = nullptr;
        QString name;
    };

    void pump();
    void updateStatus();

    QTimer m_pumpTimer;
    std::map<quint32, Pad> m_pads; // keyed by SDL_JoystickID
    QString m_status;
    bool m_initialized = false;
};

} // namespace nexora::input
