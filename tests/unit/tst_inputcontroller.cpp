#include "input/GamepadNavigator.h"
#include "input/InputBackend.h"
#include "input/InputController.h"

#include <QKeyEvent>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

#include <algorithm>

using nexora::input::GamepadButton;
using nexora::input::InputController;
using nexora::input::NavAction;

namespace {

struct KeyRecord
{
    QEvent::Type type;
    int key;
};

class RecordingWindow : public QWindow
{
public:
    QList<KeyRecord> events;

    int count(QEvent::Type type, int key) const
    {
        return static_cast<int>(std::count_if(events.cbegin(), events.cend(),
                                              [&](const KeyRecord &r) { return r.type == type && r.key == key; }));
    }

protected:
    void keyPressEvent(QKeyEvent *event) override { events.append({QEvent::KeyPress, event->key()}); }
    void keyReleaseEvent(QKeyEvent *event) override { events.append({QEvent::KeyRelease, event->key()}); }
};

} // namespace

class TestInputController : public QObject
{
    Q_OBJECT

private slots:
    void newActionsAndCapture();
    void chordWorksWhileSuspended();
    void gameBarWorksWhileSuspended();
    void acceptSendsReturn();
    void actionMapping_data();
    void actionMapping();
    void keyForAction();
    void navigatorFeedsWindow();
    void suspendedDropsActions();
    void resumeDeliversAgain();
    void nullTargetIsDropped();
    void emptyProviderIsDropped();
    void startNone();
    void startUnknownFails();
    void startAutoNeverFails();
    void restartReplacesBackend();
};

void TestInputController::acceptSendsReturn()
{
    RecordingWindow window;
    InputController controller;
    controller.setTargetWindowProvider([&] { return &window; });

    controller.injectAction(NavAction::Accept);
    QTRY_COMPARE(window.events.size(), 2);
    QCOMPARE(window.events.at(0).type, QEvent::KeyPress);
    QCOMPARE(window.events.at(0).key, int(Qt::Key_Return));
    QCOMPARE(window.events.at(1).type, QEvent::KeyRelease);
    QCOMPARE(window.events.at(1).key, int(Qt::Key_Return));
}

void TestInputController::actionMapping_data()
{
    QTest::addColumn<NavAction>("action");
    QTest::addColumn<int>("key");
    QTest::newRow("up") << NavAction::Up << int(Qt::Key_Up);
    QTest::newRow("down") << NavAction::Down << int(Qt::Key_Down);
    QTest::newRow("left") << NavAction::Left << int(Qt::Key_Left);
    QTest::newRow("right") << NavAction::Right << int(Qt::Key_Right);
    QTest::newRow("accept") << NavAction::Accept << int(Qt::Key_Return);
    QTest::newRow("back") << NavAction::Back << int(Qt::Key_Escape);
    QTest::newRow("menu") << NavAction::Menu << int(Qt::Key_Menu);
}

void TestInputController::actionMapping()
{
    QFETCH(NavAction, action);
    QFETCH(int, key);
    RecordingWindow window;
    InputController controller;
    controller.setTargetWindowProvider([&] { return &window; });

    controller.injectAction(action);
    QTRY_COMPARE(window.events.size(), 2);
    QCOMPARE(window.count(QEvent::KeyPress, key), 1);
    QCOMPARE(window.count(QEvent::KeyRelease, key), 1);
}

void TestInputController::keyForAction()
{
    QCOMPARE(InputController::keyForAction(NavAction::Up), int(Qt::Key_Up));
    QCOMPARE(InputController::keyForAction(NavAction::Accept), int(Qt::Key_Return));
    QCOMPARE(InputController::keyForAction(NavAction::Back), int(Qt::Key_Escape));
    QCOMPARE(InputController::keyForAction(NavAction::Menu), int(Qt::Key_Menu));
    QCOMPARE(InputController::keyForAction(NavAction::GameBar), int(Qt::Key_HomePage));
    QCOMPARE(InputController::keyForAction(NavAction::Options), int(Qt::Key_Context1));
}

void TestInputController::navigatorFeedsWindow()
{
    RecordingWindow window;
    InputController controller;
    controller.setTargetWindowProvider([&] { return &window; });

    controller.navigator()->buttonEvent(GamepadButton::East, true);
    controller.navigator()->buttonEvent(GamepadButton::East, false);
    QTRY_COMPARE(window.events.size(), 2);
    QCOMPARE(window.count(QEvent::KeyPress, Qt::Key_Escape), 1);
}

void TestInputController::suspendedDropsActions()
{
    RecordingWindow window;
    InputController controller;
    controller.setTargetWindowProvider([&] { return &window; });
    QSignalSpy suspendedSpy(&controller, &InputController::suspendedChanged);

    controller.setSuspended(true);
    QVERIFY(controller.isSuspended());
    QCOMPARE(suspendedSpy.count(), 1);
    controller.setSuspended(true);
    QCOMPARE(suspendedSpy.count(), 1);

    controller.injectAction(NavAction::Accept);
    controller.navigator()->buttonEvent(GamepadButton::DpadDown, true);
    QCoreApplication::processEvents();
    QTest::qWait(50);
    QVERIFY(window.events.isEmpty());
}

void TestInputController::resumeDeliversAgain()
{
    RecordingWindow window;
    InputController controller;
    controller.setTargetWindowProvider([&] { return &window; });
    controller.setProperty("suspended", true);
    controller.injectAction(NavAction::Up);
    controller.setProperty("suspended", false);
    controller.injectAction(NavAction::Down);
    QTRY_COMPARE(window.events.size(), 2);
    QCOMPARE(window.count(QEvent::KeyPress, Qt::Key_Down), 1);
    QCOMPARE(window.count(QEvent::KeyPress, Qt::Key_Up), 0);
}

void TestInputController::nullTargetIsDropped()
{
    InputController controller;
    controller.setTargetWindowProvider([] { return static_cast<QWindow *>(nullptr); });
    QSignalSpy delivered(&controller, &InputController::actionDelivered);
    controller.injectAction(NavAction::Accept);
    QCoreApplication::processEvents();
    QCOMPARE(delivered.count(), 0);
}

void TestInputController::emptyProviderIsDropped()
{
    InputController controller;
    controller.setTargetWindowProvider({});
    controller.injectAction(NavAction::Back);
    QCoreApplication::processEvents();
}

void TestInputController::startNone()
{
    InputController controller;
    QSignalSpy statusSpy(&controller, &InputController::statusChanged);
    QVERIFY(controller.start(QStringLiteral("none")));
    QCOMPARE(controller.backendName(), QStringLiteral("none"));
    QCOMPARE(controller.property("backendName").toString(), QStringLiteral("none"));
    QVERIFY(!controller.statusText().isEmpty());
    QVERIFY(controller.devices().isEmpty());
    QVERIFY(statusSpy.count() >= 1);
}

void TestInputController::startUnknownFails()
{
    InputController controller;
    QVERIFY(!controller.start(QStringLiteral("bogus")));
    QCOMPARE(controller.backendName(), QStringLiteral("none"));
}

void TestInputController::startAutoNeverFails()
{
    // Whatever the machine has (no /dev/input in a container, no permissions, ...), auto
    // must leave the controller in a usable state.
    InputController controller;
    QVERIFY(controller.start());
    const QStringList allowed{QStringLiteral("sdl3"), QStringLiteral("evdev"), QStringLiteral("none")};
    QVERIFY2(allowed.contains(controller.backendName()), qPrintable(controller.backendName()));
    QVERIFY(!controller.statusText().isEmpty());
    qInfo() << "auto backend:" << controller.backendName() << "-" << controller.statusText() << controller.devices();
}

void TestInputController::restartReplacesBackend()
{
    InputController controller;
    QVERIFY(controller.start());
    QVERIFY(controller.start(QStringLiteral("none")));
    QCOMPARE(controller.backendName(), QStringLiteral("none"));
    QCOMPARE(controller.findChildren<nexora::input::InputBackend *>().size(), 0);
}

void TestInputController::chordWorksWhileSuspended()
{
    InputController controller;
    RecordingWindow window;
    controller.setTargetWindowProvider([&window] { return &window; });
    controller.navigator()->setChordHoldDuration(std::chrono::milliseconds(20));
    QSignalSpy chord(&controller, &InputController::returnToLauncherRequested);

    controller.setSuspended(true);
    controller.navigator()->buttonEvent(GamepadButton::Select, true);
    controller.navigator()->buttonEvent(GamepadButton::Start, true);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 1, 2000);
    QCoreApplication::processEvents();
    QVERIFY(window.events.isEmpty()); // navigation (Menu) still dropped
}

void TestInputController::gameBarWorksWhileSuspended()
{
    InputController controller;
    RecordingWindow window;
    controller.setTargetWindowProvider([&window] { return &window; });
    QSignalSpy gameBar(&controller, &InputController::gameBarRequested);

    // Guide is reported as a request, never as a key, suspended (emulator running) or not.
    controller.navigator()->buttonEvent(GamepadButton::Guide, true);
    QCOMPARE(gameBar.size(), 1);
    controller.setSuspended(true);
    controller.navigator()->buttonEvent(GamepadButton::Guide, false);
    controller.navigator()->buttonEvent(GamepadButton::Guide, true);
    QCOMPARE(gameBar.size(), 2);
    QCoreApplication::processEvents();
    QTest::qWait(20);
    QVERIFY(window.events.isEmpty());

    // While a remap is being captured, Guide is captured like any other button.
    controller.setSuspended(false);
    controller.setCapturing(true);
    controller.navigator()->buttonEvent(GamepadButton::Guide, false);
    controller.navigator()->buttonEvent(GamepadButton::Guide, true);
    QCOMPARE(gameBar.size(), 2);
}

void TestInputController::newActionsAndCapture()
{
    InputController controller;
    RecordingWindow window;
    controller.setTargetWindowProvider([&window] { return &window; });
    controller.navigator()->buttonEvent(GamepadButton::North, true);
    controller.navigator()->buttonEvent(GamepadButton::R1, true);
    QTRY_COMPARE(window.count(QEvent::KeyPress, Qt::Key_Favorites), 1);
    QTRY_COMPARE(window.count(QEvent::KeyPress, Qt::Key_PageDown), 1);

    QSignalSpy captured(&controller, &InputController::buttonCaptured);
    window.events.clear();
    controller.setCapturing(true);
    controller.navigator()->buttonEvent(GamepadButton::East, true);
    controller.navigator()->buttonEvent(GamepadButton::DpadDown, true);
    QTRY_COMPARE(captured.size(), 1); // delivered queued, after the press was swallowed
    QCOMPARE(captured.first().first().toString(), QStringLiteral("east"));
    QTRY_COMPARE(window.count(QEvent::KeyPress, Qt::Key_Down), 1); // directions still navigate
    QCOMPARE(window.count(QEvent::KeyPress, Qt::Key_Escape), 0); // East not delivered as Back

    controller.setCapturing(false);
    auto mapping = controller.mapping();
    mapping.bind(NavAction::Accept, GamepadButton::East);
    controller.setMapping(mapping);
    controller.navigator()->buttonEvent(GamepadButton::East, true);
    QTRY_COMPARE(window.count(QEvent::KeyPress, Qt::Key_Return), 1);
}

QTEST_MAIN(TestInputController)
#include "tst_inputcontroller.moc"
