#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QWindow>

namespace nexora {

// Game controller support via SDL2 (if compiled with NEXORA_HAVE_SDL).
//
// Physical input is translated into LOGICAL actions, which are delivered to the
// UI as synthetic key events posted to the target window, so keyboard and
// controller share a single navigation path:
//   D-pad / left stick  -> Up/Down/Left/Right    (Qt::Key_Up/Down/Left/Right)
//   South (A / Cross)   -> Confirm               (Qt::Key_Return)
//   East  (B / Circle)  -> Back                  (Qt::Key_Escape)
//   West  (X / Square)  -> Secondary action      (Qt::Key_F)
//   North (Y / Triangle)-> Search                (Qt::Key_Slash)
//   Start               -> Options / menu        (Qt::Key_Menu)
//   LB / RB             -> Previous / next tab   (Qt::Key_PageUp / Qt::Key_PageDown)
// Directional input auto-repeats (initial delay repeatDelayMs, then every
// repeatIntervalMs); action buttons never repeat. Synthetic events are tagged
// with nativeModifiers() == SyntheticMarker so the app-wide event filter can
// tell real keyboard input apart (for button-hint labels).
// While `suppressed` is true (an emulator owns the screen) no events are posted.
class ControllerManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)                 // SDL compiled in and initialised
    Q_PROPERTY(QString unavailableReason READ unavailableReason CONSTANT)
    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY controllersChanged) // [{instanceId, name, type}]
    Q_PROPERTY(int count READ count NOTIFY controllersChanged)
    Q_PROPERTY(bool suppressed READ suppressed WRITE setSuppressed NOTIFY suppressedChanged)
    Q_PROPERTY(QString lastInputDevice READ lastInputDevice NOTIFY lastInputDeviceChanged) // "keyboard" | "controller"
    Q_PROPERTY(QString controllerType READ controllerType NOTIFY lastInputDeviceChanged)   // "xbox" | "playstation" | "nintendo" | "generic"
    Q_PROPERTY(int repeatDelayMs READ repeatDelayMs WRITE setRepeatDelayMs NOTIFY repeatChanged)
    Q_PROPERTY(int repeatIntervalMs READ repeatIntervalMs WRITE setRepeatIntervalMs NOTIFY repeatChanged)
    Q_PROPERTY(QVariantList mappings READ mappings CONSTANT)          // [{action, label, controllerInput, keyboardInput}]

public:
    static constexpr quint32 SyntheticMarker = 0x4E58; // "NX"

    explicit ControllerManager(QObject *parent = nullptr);
    ~ControllerManager() override;

    bool available() const;
    QString unavailableReason() const;
    QVariantList controllers() const;
    int count() const;
    bool suppressed() const;
    void setSuppressed(bool value);
    QString lastInputDevice() const;
    QString controllerType() const;
    int repeatDelayMs() const;
    void setRepeatDelayMs(int ms);
    int repeatIntervalMs() const;
    void setRepeatIntervalMs(int ms);
    QVariantList mappings() const;

    void setTargetWindow(QWindow *window);
    // Called by an application-wide event filter when a real key press arrives.
    void noteKeyboardInput();

    // Label for a logical action ("confirm","back","secondary","search","options","prevTab","nextTab")
    // on the current input device: e.g. "Enter"/"Esc" for keyboard, "A"/"Cross"/"B" for controllers.
    Q_INVOKABLE QString buttonLabel(const QString &action) const;

    // Feeds a logical action as if it came from a controller (used by tests and
    // the input test page). Respects `suppressed`.
    Q_INVOKABLE void injectAction(const QString &action);

signals:
    void controllersChanged();
    void controllerConnected(const QString &name);
    void controllerDisconnected(const QString &name);
    void suppressedChanged();
    void lastInputDeviceChanged();
    void repeatChanged();
    // Raw physical input for the controller test screen. `button` is a stable
    // name: "south","east","west","north","start","back","guide","leftshoulder",
    // "rightshoulder","leftstick","rightstick","dpup","dpdown","dpleft","dpright".
    void buttonEvent(const QString &button, bool pressed);
    void axisEvent(const QString &axis, qreal value); // "leftx","lefty","rightx","righty","lefttrigger","righttrigger", -1..1

private:
    void poll();
    void postKey(int qtKey);
    void setLastInputDevice(const QString &device, const QString &type = {});
    void startRepeat(int qtKey);
    void stopRepeat();
    void openController(int deviceIndex);
    void closeController(int instanceId);
    void handleButton(int instanceId, int button, bool pressed);
    void handleAxis(int instanceId, int axis, qreal value);
    QString typeOf(int instanceId) const;

    QPointer<QWindow> m_window;
    QTimer m_pollTimer;
    QTimer m_repeatTimer;
    int m_repeatKey = 0;
    bool m_available = false;
    QString m_unavailableReason;
    bool m_suppressed = false;
    QString m_lastInput = QStringLiteral("keyboard");
    QString m_controllerType = QStringLiteral("generic");
    int m_repeatDelayMs = 400;
    int m_repeatIntervalMs = 110;
    QVariantList m_controllers;
    QHash<int, void *> m_handles; // SDL instance id -> SDL_GameController*
    int m_stickDirection = 0;     // currently held stick direction key (0 = none)
};

} // namespace nexora
