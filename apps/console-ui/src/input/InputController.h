#pragma once

#include "input/ButtonMapping.h"
#include "input/GamepadInput.h"
#include "input/NavAction.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <chrono>
#include <functional>
#include <memory>

class QWindow;

namespace nexora::input {

class GamepadNavigator;
class InputBackend;

// Owns the gamepad navigator and (at most) one input backend, and turns navigation
// actions into synthesized key presses delivered to the focused window, so the QML UI only
// has to handle ordinary keyboard navigation.
//
// It also keeps the list of connected gamepads (devices()), announcing changes through
// deviceConnected()/deviceDisconnected(), and offers a test mode in which input is reported
// as text instead of navigating (setTestMode()).
class InputController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backendName READ backendName NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool suspended READ isSuspended WRITE setSuspended NOTIFY suspendedChanged)
    Q_PROPERTY(bool testMode READ isTestMode WRITE setTestMode NOTIFY testModeChanged)
    Q_PROPERTY(QString lastRawEvent READ lastRawEvent NOTIFY rawEvent)

public:
    // How long Back has to be held to leave test mode.
    static constexpr std::chrono::milliseconds kTestModeExitHold{2000};

    explicit InputController(QObject *parent = nullptr);
    ~InputController() override;

    // Starts a backend: "auto" (SDL3 if built and working, else evdev, else none), "evdev",
    // "sdl3" or "none". Any previously running backend is stopped first; two backends never
    // run at once. Returns false if the requested backend is unknown, not built or failed to
    // start (the controller then runs with backend "none").
    bool start(const QString &backend = QStringLiteral("auto"));

    // Starts a caller-provided backend (tests, future platform backends). It must feed
    // navigator(). Any running backend is stopped first.
    bool start(std::unique_ptr<InputBackend> backend);

    QString backendName() const;
    QString statusText() const;
    // One entry per connected gamepad: {id, name, backend, connectedAt} (connectedAt is an
    // ISO-8601 UTC timestamp taken when this controller first saw the device).
    QVariantList devices() const { return m_devices; }

    // While suspended (e.g. an emulator is in the foreground) actions are dropped.
    bool isSuspended() const { return m_suspended; }
    void setSuspended(bool suspended);

    // Applies a button mapping (validated; see ButtonMapping).
    void setMapping(const ButtonMapping &mapping);
    ButtonMapping mapping() const;

    // While capturing, button presses are reported through buttonCaptured() instead of
    // being turned into actions (used by the remapping screen). Directions still navigate.
    bool isCapturing() const { return m_capturing; }
    void setCapturing(bool capturing);

    // In test mode nothing navigates: every button edge and axis reading is reported as
    // text through rawEvent() / lastRawEvent ("south pressed", "LeftX -0.82"). The only way
    // out from a controller is holding Back for testModeExitHold(), which turns test mode
    // off and emits testModeExited().
    Q_INVOKABLE void setTestMode(bool enabled);
    bool isTestMode() const { return m_testMode; }
    QString lastRawEvent() const { return m_lastRawEvent; }
    // Shortened by tests; clamped to at least 1 ms.
    void setTestModeExitHold(std::chrono::milliseconds duration);
    std::chrono::milliseconds testModeExitHold() const { return m_testExitHold; }

    // Overrides where synthesized key events go (default: QGuiApplication::focusWindow()).
    void setTargetWindowProvider(std::function<QWindow *()> provider);

    // Delivers |action| exactly as if it came from a gamepad (respects suspension).
    Q_INVOKABLE void injectAction(nexora::input::NavAction action);

    GamepadNavigator *navigator() const { return m_navigator; }

    // Qt::Key used for |action|.
    static int keyForAction(NavAction action);

signals:
    void statusChanged();
    void suspendedChanged();
    void devicesChanged();
    // A gamepad appeared / went away. Emitted even while suspended, so a running emulator
    // can be told about it without input being resumed.
    void deviceConnected(const QString &name);
    void deviceDisconnected(const QString &name);
    void testModeChanged();
    // Test mode only: one button edge or axis reading, as text.
    void rawEvent(const QString &text);
    // Back was held long enough in test mode; test mode is already off.
    void testModeExited();
    // Emitted for every action that was delivered to a window.
    void actionDelivered(nexora::input::NavAction action);
    // The "return to launcher" chord (hold Select + Start) was completed. Emitted even while
    // suspended: this is how a controller-only user leaves a running emulator.
    void returnToLauncherRequested();
    void buttonCaptured(const QString &buttonName);

private:
    bool startBackend(std::unique_ptr<InputBackend> backend);
    void stopBackend();
    void deliver(NavAction action);
    // Re-reads the backend's device list and announces what changed.
    void refreshDevices();
    void report(const QString &text);
    GamepadButton backButton() const;

    GamepadNavigator *m_navigator;
    InputBackend *m_backend = nullptr;
    std::function<QWindow *()> m_targetProvider;
    QString m_statusOverride; // used when no backend runs
    QVariantList m_devices; // {id, name, backend, connectedAt} per connected gamepad
    QString m_lastRawEvent;
    QTimer m_testExitTimer;
    std::chrono::milliseconds m_testExitHold{kTestModeExitHold};
    bool m_suspended = false;
    bool m_capturing = false;
    bool m_testMode = false;
};

} // namespace nexora::input
