#pragma once

#include "input/NavAction.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QWindow;

namespace nexora::input {

class GamepadNavigator;
class InputBackend;

// Owns the gamepad navigator and (at most) one input backend, and turns navigation
// actions into synthesized key presses delivered to the focused window, so the QML UI only
// has to handle ordinary keyboard navigation.
class InputController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString backendName READ backendName NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QStringList devices READ devices NOTIFY statusChanged)
    Q_PROPERTY(bool suspended READ isSuspended WRITE setSuspended NOTIFY suspendedChanged)

public:
    explicit InputController(QObject *parent = nullptr);
    ~InputController() override;

    // Starts a backend: "auto" (SDL3 if built and working, else evdev, else none), "evdev",
    // "sdl3" or "none". Any previously running backend is stopped first; two backends never
    // run at once. Returns false if the requested backend is unknown, not built or failed to
    // start (the controller then runs with backend "none").
    bool start(const QString &backend = QStringLiteral("auto"));

    QString backendName() const;
    QString statusText() const;
    QStringList devices() const;

    // While suspended (e.g. an emulator is in the foreground) actions are dropped.
    bool isSuspended() const { return m_suspended; }
    void setSuspended(bool suspended);

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
    // Emitted for every action that was delivered to a window.
    void actionDelivered(nexora::input::NavAction action);

private:
    bool startBackend(std::unique_ptr<InputBackend> backend);
    void stopBackend();
    void deliver(NavAction action);

    GamepadNavigator *m_navigator;
    InputBackend *m_backend = nullptr;
    std::function<QWindow *()> m_targetProvider;
    QString m_statusOverride; // used when no backend runs
    bool m_suspended = false;
};

} // namespace nexora::input
