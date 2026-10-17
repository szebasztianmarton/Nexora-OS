#include "input/GamepadNavigator.h"

#ifdef NEXORA_HAVE_EVDEV
#include "input/EvdevMapping.h"
#include <linux/input.h>
#endif

#include <QSignalSpy>
#include <QTest>

#include <cmath>

using namespace std::chrono_literals;
using nexora::input::GamepadAxis;
using nexora::input::GamepadButton;
using nexora::input::GamepadNavigator;
using nexora::input::NavAction;

namespace {

QList<NavAction> actions(const QSignalSpy &spy)
{
    QList<NavAction> result;
    for (const auto &args : spy)
        result.append(args.at(0).value<NavAction>());
    return result;
}

} // namespace

class TestGamepadNavigator : public QObject
{
    Q_OBJECT

private slots:
    void buttonMapping_data();
    void buttonMapping();
    void releaseDoesNotTrigger();
    void unmappedButtonsIgnored();
    void axisThresholdAndHysteresis();
    void axisDirections();
    void axisFlipToOppositeSide();
    void hatAxes();
    void heldDirectionRepeats();
    void nonDirectionDoesNotRepeat();
    void releaseStopsRepeat();
    void latestDirectionWins();
    void resetStopsRepeat();
    void resetClearsAxisState();
    void defaultRepeatDelays();
    void chordFiresAfterHold();
    void chordCancelledByEarlyRelease();
    void chordNeedsBothButtons();
    void devicesDoNotInterfere();
    void chordIsPerDevice();
    void startHoldRequestsQuickMenu();
    void startHoldWithSelectIsTheChordNotTheMenu();
    void resetDeviceDropsOnlyThatDevice();
    void defaultDeadzoneMatchesSettings();
    void deadzoneThresholdFormulas();
    void deadzoneAppliesToAxisEdges();
    void largeDeadzoneIgnoresMidTravel();
    void setDeadzoneClamps();
    void repeatedPressWithoutReleaseActsOnce();
    void rawEventsAreReportedBeforeThresholds();
#ifdef NEXORA_HAVE_EVDEV
    void evdevKeyMapping();
    void evdevLayoutSelection();
    void evdevXboxLabelsSwapWestAndNorth();
    void evdevJoystickLayouts();
    void evdevAbsMapping();
    void normalizeAxisValues();
#endif
};

void TestGamepadNavigator::buttonMapping_data()
{
    QTest::addColumn<GamepadButton>("button");
    QTest::addColumn<NavAction>("action");
    QTest::newRow("south") << GamepadButton::South << NavAction::Accept;
    QTest::newRow("east") << GamepadButton::East << NavAction::Back;
    QTest::newRow("start") << GamepadButton::Start << NavAction::Menu;
    QTest::newRow("guide") << GamepadButton::Guide << NavAction::GameBar;
    QTest::newRow("select") << GamepadButton::Select << NavAction::Options;
    QTest::newRow("north") << GamepadButton::North << NavAction::Favorite;
    QTest::newRow("west") << GamepadButton::West << NavAction::Search;
    QTest::newRow("l1") << GamepadButton::L1 << NavAction::PrevTab;
    QTest::newRow("r1") << GamepadButton::R1 << NavAction::NextTab;
    QTest::newRow("dpad-up") << GamepadButton::DpadUp << NavAction::Up;
    QTest::newRow("dpad-down") << GamepadButton::DpadDown << NavAction::Down;
    QTest::newRow("dpad-left") << GamepadButton::DpadLeft << NavAction::Left;
    QTest::newRow("dpad-right") << GamepadButton::DpadRight << NavAction::Right;
}

void TestGamepadNavigator::buttonMapping()
{
    QFETCH(GamepadButton, button);
    QFETCH(NavAction, action);
    GamepadNavigator nav;
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(button, true);
    QCOMPARE(actions(spy), QList<NavAction>{action});
}

void TestGamepadNavigator::releaseDoesNotTrigger()
{
    GamepadNavigator nav;
    nav.buttonEvent(GamepadButton::South, true);
    nav.buttonEvent(GamepadButton::DpadUp, true);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::South, false);
    nav.buttonEvent(GamepadButton::DpadUp, false);
    QCOMPARE(spy.count(), 0);
}

void TestGamepadNavigator::unmappedButtonsIgnored()
{
    GamepadNavigator nav;
    QVariantMap minimal{{QStringLiteral("accept"), QStringLiteral("south")},
                        {QStringLiteral("back"), QStringLiteral("east")}};
    nav.setMapping(*nexora::input::ButtonMapping::fromVariantMap(minimal, nullptr));
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    QSignalSpy raw(&nav, &GamepadNavigator::buttonPressed);
    nav.buttonEvent(GamepadButton::West, true);
    nav.buttonEvent(GamepadButton::North, true);
    nav.buttonEvent(GamepadButton::L1, true);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(raw.count(), 3); // still reported for remap capture
}

void TestGamepadNavigator::axisThresholdAndHysteresis()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(10s, 10s); // keep repeat out of the picture
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);

    nav.axisEvent(GamepadAxis::LeftX, 0.4);
    QCOMPARE(spy.count(), 0);
    nav.axisEvent(GamepadAxis::LeftX, 0.6);
    QCOMPARE(actions(spy), QList<NavAction>{NavAction::Right});
    nav.axisEvent(GamepadAxis::LeftX, 0.9);
    nav.axisEvent(GamepadAxis::LeftX, 0.4); // inside hysteresis band: still held
    QCOMPARE(spy.count(), 1);
    QCOMPARE(nav.repeatingAction(), std::optional<NavAction>(NavAction::Right));
    nav.axisEvent(GamepadAxis::LeftX, 0.2); // released
    QCOMPARE(nav.repeatingAction(), std::nullopt);
    nav.axisEvent(GamepadAxis::LeftX, 0.4);
    QCOMPARE(spy.count(), 1);
    nav.axisEvent(GamepadAxis::LeftX, 0.6); // pressed again
    QCOMPARE(spy.count(), 2);
}

void TestGamepadNavigator::axisDirections()
{
    GamepadNavigator nav;
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::LeftX, -0.7);
    nav.axisEvent(GamepadAxis::LeftX, 0.0);
    nav.axisEvent(GamepadAxis::LeftY, -0.7);
    nav.axisEvent(GamepadAxis::LeftY, 0.0);
    nav.axisEvent(GamepadAxis::LeftY, 1.0);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Left, NavAction::Up, NavAction::Down}));
}

void TestGamepadNavigator::axisFlipToOppositeSide()
{
    GamepadNavigator nav;
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::LeftX, 1.0);
    nav.axisEvent(GamepadAxis::LeftX, -1.0);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Right, NavAction::Left}));
    QCOMPARE(nav.repeatingAction(), std::optional<NavAction>(NavAction::Left));
}

void TestGamepadNavigator::hatAxes()
{
    GamepadNavigator nav;
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::HatY, -1.0);
    nav.axisEvent(GamepadAxis::HatY, 0.0);
    nav.axisEvent(GamepadAxis::HatY, 1.0);
    nav.axisEvent(GamepadAxis::HatY, 0.0);
    nav.axisEvent(GamepadAxis::HatX, -1.0);
    nav.axisEvent(GamepadAxis::HatX, 0.0);
    nav.axisEvent(GamepadAxis::HatX, 1.0);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Up, NavAction::Down, NavAction::Left, NavAction::Right}));
}

void TestGamepadNavigator::heldDirectionRepeats()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::DpadDown, true);
    QCOMPARE(spy.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 3, 2000);
    for (NavAction a : actions(spy))
        QCOMPARE(a, NavAction::Down);
}

void TestGamepadNavigator::nonDirectionDoesNotRepeat()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::South, true);
    nav.buttonEvent(GamepadButton::East, true);
    nav.buttonEvent(GamepadButton::Start, true);
    QTest::qWait(150);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Accept, NavAction::Back, NavAction::Menu}));
}

void TestGamepadNavigator::releaseStopsRepeat()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::LeftY, -1.0);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 2, 2000);
    nav.axisEvent(GamepadAxis::LeftY, 0.0);
    const auto count = spy.count();
    QTest::qWait(120);
    QCOMPARE(spy.count(), count);
}

void TestGamepadNavigator::latestDirectionWins()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::DpadUp, true);
    nav.buttonEvent(GamepadButton::DpadRight, true);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 4, 2000);
    const auto got = actions(spy);
    QCOMPARE(got.at(0), NavAction::Up);
    for (qsizetype i = 1; i < got.size(); ++i)
        QCOMPARE(got.at(i), NavAction::Right);

    // Releasing the older direction does not disturb the repeating one.
    nav.buttonEvent(GamepadButton::DpadUp, false);
    QCOMPARE(nav.repeatingAction(), std::optional<NavAction>(NavAction::Right));
}

void TestGamepadNavigator::resetStopsRepeat()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::DpadLeft, true);
    QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 2, 2000);
    nav.reset();
    QCOMPARE(nav.repeatingAction(), std::nullopt);
    const auto count = spy.count();
    QTest::qWait(120);
    QCOMPARE(spy.count(), count);
}

void TestGamepadNavigator::resetClearsAxisState()
{
    GamepadNavigator nav;
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::LeftX, 1.0);
    nav.reset();
    // After a reset (e.g. SYN_DROPPED) the next strong reading counts as a fresh press.
    nav.axisEvent(GamepadAxis::LeftX, 1.0);
    QCOMPARE(spy.count(), 2);
}

void TestGamepadNavigator::defaultRepeatDelays()
{
    GamepadNavigator nav;
    QCOMPARE(nav.initialRepeatDelay(), 400ms);
    QCOMPARE(nav.repeatInterval(), 120ms);
}

#ifdef NEXORA_HAVE_EVDEV
void TestGamepadNavigator::evdevKeyMapping()
{
    using nexora::input::evdevKeyToButton;
    QCOMPARE(evdevKeyToButton(BTN_SOUTH), std::optional(GamepadButton::South));
    QCOMPARE(evdevKeyToButton(BTN_A), std::optional(GamepadButton::South));
    QCOMPARE(evdevKeyToButton(BTN_EAST), std::optional(GamepadButton::East));
    QCOMPARE(evdevKeyToButton(BTN_B), std::optional(GamepadButton::East));
    QCOMPARE(evdevKeyToButton(BTN_NORTH), std::optional(GamepadButton::North));
    QCOMPARE(evdevKeyToButton(BTN_WEST), std::optional(GamepadButton::West));
    QCOMPARE(evdevKeyToButton(BTN_START), std::optional(GamepadButton::Start));
    QCOMPARE(evdevKeyToButton(BTN_SELECT), std::optional(GamepadButton::Select));
    QCOMPARE(evdevKeyToButton(BTN_MODE), std::optional(GamepadButton::Guide));
    QCOMPARE(evdevKeyToButton(BTN_DPAD_UP), std::optional(GamepadButton::DpadUp));
    QCOMPARE(evdevKeyToButton(BTN_DPAD_DOWN), std::optional(GamepadButton::DpadDown));
    QCOMPARE(evdevKeyToButton(BTN_DPAD_LEFT), std::optional(GamepadButton::DpadLeft));
    QCOMPARE(evdevKeyToButton(BTN_DPAD_RIGHT), std::optional(GamepadButton::DpadRight));
    QCOMPARE(evdevKeyToButton(KEY_A), std::nullopt);
    QCOMPARE(evdevKeyToButton(BTN_LEFT), std::nullopt);
    QCOMPARE(evdevKeyToButton(BTN_TL), std::optional(GamepadButton::L1));
    QCOMPARE(evdevKeyToButton(BTN_TR), std::optional(GamepadButton::R1));
    QCOMPARE(evdevKeyToButton(BTN_TL2), std::nullopt);
}

// Layout per family of docs/linux_gamepad_drivers.md; the driver names are the kernel's
// (drivers/input/joystick/xpad.c, drivers/hid/hid-*.c).
void TestGamepadNavigator::evdevLayoutSelection()
{
    using nexora::input::EvdevLayout;
    using nexora::input::evdevLayout;
    // Xbox family: xpad (any vendor, e.g. PDP 0e6f), Microsoft over Bluetooth, Steam.
    QCOMPARE(evdevLayout(true, 0x0e6f, 0x0113, QStringLiteral("xpad")), EvdevLayout::XboxLabels);
    QCOMPARE(evdevLayout(true, 0x045e, 0x0b13, QStringLiteral("hid-generic")), EvdevLayout::XboxLabels);
    QCOMPARE(evdevLayout(true, 0x045e, 0x028e, QString()), EvdevLayout::XboxLabels); // uinput: no driver
    QCOMPARE(evdevLayout(true, 0x28de, 0x1102, QStringLiteral("hid-steam")), EvdevLayout::XboxLabels);
    // Drivers that already report positions.
    QCOMPARE(evdevLayout(true, 0x054c, 0x0ce6, QStringLiteral("playstation")), EvdevLayout::Positional);
    QCOMPARE(evdevLayout(true, 0x054c, 0x0268, QStringLiteral("sony")), EvdevLayout::Positional);
    QCOMPARE(evdevLayout(true, 0x057e, 0x2009, QStringLiteral("nintendo")), EvdevLayout::Positional);
    QCOMPARE(evdevLayout(true, 0x2dc8, 0x6101, QStringLiteral("hid-generic")), EvdevLayout::Positional);
    // No BTN_GAMEPAD: numbered buttons.
    QCOMPARE(evdevLayout(false, 0x046d, 0xc216, QStringLiteral("logitech")), EvdevLayout::Joystick);
    QCOMPARE(evdevLayout(false, 0x044f, 0xb10a, QStringLiteral("thrustmaster")), EvdevLayout::Joystick);
    QCOMPARE(evdevLayout(false, 0x0079, 0x0006, QStringLiteral("dragonrise")), EvdevLayout::JoystickPlayStation);
    QCOMPARE(evdevLayout(false, 0x0079, 0x0011, QStringLiteral("hid-generic")), EvdevLayout::Joystick);
}

// xpad reports the Xbox X (left) button as BTN_X, which the kernel defines as BTN_NORTH.
void TestGamepadNavigator::evdevXboxLabelsSwapWestAndNorth()
{
    using nexora::input::evdevKeyToButton;
    using nexora::input::EvdevLayout;
    QCOMPARE(evdevKeyToButton(BTN_X, EvdevLayout::XboxLabels), std::optional(GamepadButton::West));
    QCOMPARE(evdevKeyToButton(BTN_Y, EvdevLayout::XboxLabels), std::optional(GamepadButton::North));
    QCOMPARE(evdevKeyToButton(BTN_A, EvdevLayout::XboxLabels), std::optional(GamepadButton::South));
    QCOMPARE(evdevKeyToButton(BTN_B, EvdevLayout::XboxLabels), std::optional(GamepadButton::East));
    QCOMPARE(evdevKeyToButton(BTN_MODE, EvdevLayout::XboxLabels), std::optional(GamepadButton::Guide));
    QCOMPARE(evdevKeyToButton(BTN_TL, EvdevLayout::XboxLabels), std::optional(GamepadButton::L1));
    // Positional drivers keep the kernel meaning.
    QCOMPARE(evdevKeyToButton(BTN_NORTH, EvdevLayout::Positional), std::optional(GamepadButton::North));
    QCOMPARE(evdevKeyToButton(BTN_WEST, EvdevLayout::Positional), std::optional(GamepadButton::West));
}

// Numbered buttons, after SDL's gamecontrollerdb: Logitech Dual Action (x:b0 a:b1 b:b2 y:b3)
// and DragonRise "USB Gamepad" (y:b0 b:b1 a:b2 x:b3); both 5/6 shoulders, 9 back, 10 start.
void TestGamepadNavigator::evdevJoystickLayouts()
{
    using nexora::input::evdevKeyToButton;
    using nexora::input::EvdevLayout;
    const auto pc = EvdevLayout::Joystick;
    QCOMPARE(evdevKeyToButton(BTN_TRIGGER, pc), std::optional(GamepadButton::West));
    QCOMPARE(evdevKeyToButton(BTN_THUMB, pc), std::optional(GamepadButton::South));
    QCOMPARE(evdevKeyToButton(BTN_THUMB2, pc), std::optional(GamepadButton::East));
    QCOMPARE(evdevKeyToButton(BTN_TOP, pc), std::optional(GamepadButton::North));
    QCOMPARE(evdevKeyToButton(BTN_TOP2, pc), std::optional(GamepadButton::L1));
    QCOMPARE(evdevKeyToButton(BTN_PINKIE, pc), std::optional(GamepadButton::R1));
    QCOMPARE(evdevKeyToButton(BTN_BASE, pc), std::nullopt); // left trigger
    QCOMPARE(evdevKeyToButton(BTN_BASE2, pc), std::nullopt); // right trigger
    QCOMPARE(evdevKeyToButton(BTN_BASE3, pc), std::optional(GamepadButton::Select));
    QCOMPARE(evdevKeyToButton(BTN_BASE4, pc), std::optional(GamepadButton::Start));
    QCOMPARE(evdevKeyToButton(BTN_BASE5, pc), std::nullopt);

    const auto ps = EvdevLayout::JoystickPlayStation;
    QCOMPARE(evdevKeyToButton(BTN_TRIGGER, ps), std::optional(GamepadButton::North));
    QCOMPARE(evdevKeyToButton(BTN_THUMB, ps), std::optional(GamepadButton::East));
    QCOMPARE(evdevKeyToButton(BTN_THUMB2, ps), std::optional(GamepadButton::South));
    QCOMPARE(evdevKeyToButton(BTN_TOP, ps), std::optional(GamepadButton::West));
    QCOMPARE(evdevKeyToButton(BTN_TOP2, ps), std::optional(GamepadButton::L1));
    QCOMPARE(evdevKeyToButton(BTN_BASE4, ps), std::optional(GamepadButton::Start));

    // Joystick buttons mean nothing to gamepad layouts.
    QCOMPARE(evdevKeyToButton(BTN_TRIGGER, EvdevLayout::Positional), std::nullopt);
    QCOMPARE(evdevKeyToButton(BTN_TRIGGER, EvdevLayout::XboxLabels), std::nullopt);
}

void TestGamepadNavigator::evdevAbsMapping()
{
    using nexora::input::evdevAbsToAxis;
    QCOMPARE(evdevAbsToAxis(ABS_X), std::optional(GamepadAxis::LeftX));
    QCOMPARE(evdevAbsToAxis(ABS_Y), std::optional(GamepadAxis::LeftY));
    QCOMPARE(evdevAbsToAxis(ABS_HAT0X), std::optional(GamepadAxis::HatX));
    QCOMPARE(evdevAbsToAxis(ABS_HAT0Y), std::optional(GamepadAxis::HatY));
    QCOMPARE(evdevAbsToAxis(ABS_RX), std::nullopt);
    QCOMPARE(evdevAbsToAxis(ABS_Z), std::nullopt);
}

void TestGamepadNavigator::normalizeAxisValues()
{
    using nexora::input::normalizeAxis;
    QCOMPARE(normalizeAxis(-32768, -32768, 32767), -1.0);
    QCOMPARE(normalizeAxis(32767, -32768, 32767), 1.0);
    QVERIFY(std::abs(normalizeAxis(0, -32768, 32767)) < 0.001);
    QCOMPARE(normalizeAxis(0, 0, 255), -1.0);
    QCOMPARE(normalizeAxis(255, 0, 255), 1.0);
    QVERIFY(std::abs(normalizeAxis(128, 0, 255)) < 0.01);
    QCOMPARE(normalizeAxis(-1, -1, 1), -1.0);
    QCOMPARE(normalizeAxis(0, -1, 1), 0.0);
    QCOMPARE(normalizeAxis(1, -1, 1), 1.0);
    QCOMPARE(normalizeAxis(500, 0, 255), 1.0); // clamped
    QCOMPARE(normalizeAxis(-500, 0, 255), -1.0); // clamped
    QCOMPARE(normalizeAxis(5, 3, 3), 0.0); // degenerate range
    QCOMPARE(normalizeAxis(5, 10, 3), 0.0);
}
#endif

void TestGamepadNavigator::chordFiresAfterHold()
{
    GamepadNavigator nav;
    QCOMPARE(nav.chordHoldDuration(), 1000ms);
    nav.setChordHoldDuration(50ms);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::Select, true);
    nav.buttonEvent(GamepadButton::Start, true);
    QCOMPARE(chord.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 1, 2000);
    QTest::qWait(150);
    QCOMPARE(chord.size(), 1); // once per hold

    nav.buttonEvent(GamepadButton::Start, false);
    nav.buttonEvent(GamepadButton::Start, true);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 2, 2000);
}

void TestGamepadNavigator::startHoldRequestsQuickMenu()
{
    GamepadNavigator nav;
    QCOMPARE(nav.menuHoldDuration(), 1000ms);
    nav.setMenuHoldDuration(50ms);
    QSignalSpy menu(&nav, &GamepadNavigator::quickMenuRequested);
    QSignalSpy actions(&nav, &GamepadNavigator::actionTriggered);
    nav.buttonEvent(GamepadButton::Start, true);
    QCOMPARE(actions.size(), 1); // the normal Menu action still fires on the press edge
    QCOMPARE(menu.size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(menu.size(), 1, 2000);
    QTest::qWait(150);
    QCOMPARE(menu.size(), 1); // once per hold

    // A short press does not open it.
    nav.buttonEvent(GamepadButton::Start, false);
    nav.setMenuHoldDuration(300ms);
    nav.buttonEvent(GamepadButton::Start, true);
    nav.buttonEvent(GamepadButton::Start, false);
    QTest::qWait(450);
    QCOMPARE(menu.size(), 1);
}

void TestGamepadNavigator::startHoldWithSelectIsTheChordNotTheMenu()
{
    GamepadNavigator nav;
    nav.setMenuHoldDuration(80ms);
    nav.setChordHoldDuration(80ms);
    QSignalSpy menu(&nav, &GamepadNavigator::quickMenuRequested);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::Select, true);
    nav.buttonEvent(GamepadButton::Start, true);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 1, 2000);
    QTest::qWait(150);
    QCOMPARE(menu.size(), 0);
}

void TestGamepadNavigator::chordCancelledByEarlyRelease()
{
    GamepadNavigator nav;
    nav.setChordHoldDuration(150ms);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::Start, true);
    nav.buttonEvent(GamepadButton::Select, true);
    QTest::qWait(30);
    nav.buttonEvent(GamepadButton::Select, false);
    QTest::qWait(300);
    QCOMPARE(chord.size(), 0);

    nav.buttonEvent(GamepadButton::Select, true);
    nav.reset(); // e.g. device disconnected
    QTest::qWait(300);
    QCOMPARE(chord.size(), 0);
}

void TestGamepadNavigator::chordNeedsBothButtons()
{
    GamepadNavigator nav;
    nav.setChordHoldDuration(20ms);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::Start, true);
    nav.buttonEvent(GamepadButton::Guide, true);
    nav.buttonEvent(GamepadButton::South, true);
    QTest::qWait(150);
    QCOMPARE(chord.size(), 0);
}

void TestGamepadNavigator::devicesDoNotInterfere()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(30ms, 15ms);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    nav.axisEvent(GamepadAxis::LeftX, 1.0, 1); // pad 1 holds right
    QCOMPARE(actions(spy), QList<NavAction>{NavAction::Right});
    nav.axisEvent(GamepadAxis::LeftX, 0.05, 2); // pad 2 stick noise near centre
    QCOMPARE(nav.repeatingAction(), std::optional<NavAction>(NavAction::Right));
    QTRY_VERIFY_WITH_TIMEOUT(spy.size() >= 3, 2000); // still repeating
    nav.axisEvent(GamepadAxis::LeftX, 0.0, 1);
    QVERIFY(!nav.repeatingAction());
}

void TestGamepadNavigator::chordIsPerDevice()
{
    GamepadNavigator nav;
    nav.setChordHoldDuration(20ms);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::Select, true, 1);
    nav.buttonEvent(GamepadButton::Start, true, 2); // different pads: not a chord
    QTest::qWait(120);
    QCOMPARE(chord.size(), 0);
    nav.buttonEvent(GamepadButton::Start, true, 1);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 1, 2000);
}

void TestGamepadNavigator::resetDeviceDropsOnlyThatDevice()
{
    GamepadNavigator nav;
    nav.setChordHoldDuration(60ms);
    QSignalSpy chord(&nav, &GamepadNavigator::returnToLauncherRequested);
    nav.buttonEvent(GamepadButton::DpadUp, true, 1);
    nav.buttonEvent(GamepadButton::Select, true, 2);
    nav.buttonEvent(GamepadButton::Start, true, 2);
    nav.resetDevice(1);
    QTRY_COMPARE_WITH_TIMEOUT(chord.size(), 1, 2000); // pad 2's chord survives
    nav.buttonEvent(GamepadButton::DpadDown, true, 2);
    nav.resetDevice(2);
    QVERIFY(!nav.repeatingAction());
}

void TestGamepadNavigator::defaultDeadzoneMatchesSettings()
{
    GamepadNavigator nav;
    QCOMPARE(nav.deadzone(), 0.25); // Settings::InputDeadzone defaults to 25%
}

void TestGamepadNavigator::deadzoneThresholdFormulas()
{
    GamepadNavigator nav;
    nav.setDeadzone(0.25);
    // press = deadzone + (1 - deadzone) * 0.4, release = deadzone + (1 - deadzone) * 0.15
    QCOMPARE(nav.axisPressThreshold(), 0.55);
    QCOMPARE(nav.axisReleaseThreshold(), 0.3625);
    nav.setDeadzone(0.5);
    QCOMPARE(nav.axisPressThreshold(), 0.7);
    QCOMPARE(nav.axisReleaseThreshold(), 0.575);
}

void TestGamepadNavigator::deadzoneAppliesToAxisEdges()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(10s, 10s); // keep repeat out of the picture
    nav.setDeadzone(0.25);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);

    nav.axisEvent(GamepadAxis::LeftX, 0.2); // inside the dead zone: treated as centred
    QCOMPARE(spy.count(), 0);
    nav.axisEvent(GamepadAxis::LeftX, 0.6); // >= 0.55: pressed
    QCOMPARE(actions(spy), QList<NavAction>{NavAction::Right});
    nav.axisEvent(GamepadAxis::LeftX, 0.4); // inside the hysteresis band: still held
    QCOMPARE(nav.repeatingAction(), std::optional<NavAction>(NavAction::Right));
    nav.axisEvent(GamepadAxis::LeftX, 0.3); // < 0.3625: released
    QCOMPARE(nav.repeatingAction(), std::nullopt);
    QCOMPARE(spy.count(), 1);
}

void TestGamepadNavigator::largeDeadzoneIgnoresMidTravel()
{
    GamepadNavigator nav;
    nav.setRepeatDelays(10s, 10s);
    nav.setDeadzone(0.5); // press 0.7, release 0.575
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);

    nav.axisEvent(GamepadAxis::LeftY, -0.6);
    QCOMPARE(spy.count(), 0);
    nav.axisEvent(GamepadAxis::LeftY, -0.75);
    QCOMPARE(actions(spy), QList<NavAction>{NavAction::Up});
    nav.axisEvent(GamepadAxis::LeftY, -0.6); // still held
    QCOMPARE(spy.count(), 1);
    nav.axisEvent(GamepadAxis::LeftY, -0.55);
    QCOMPARE(nav.repeatingAction(), std::nullopt);
}

void TestGamepadNavigator::setDeadzoneClamps()
{
    GamepadNavigator nav;
    nav.setDeadzone(0.0);
    QCOMPARE(nav.deadzone(), GamepadNavigator::kMinDeadzone);
    QCOMPARE(nav.deadzone(), 0.05);
    nav.setDeadzone(-3.0);
    QCOMPARE(nav.deadzone(), 0.05);
    nav.setDeadzone(1.0);
    QCOMPARE(nav.deadzone(), GamepadNavigator::kMaxDeadzone);
    QCOMPARE(nav.deadzone(), 0.6);
    nav.setDeadzone(std::nan(""));
    QCOMPARE(nav.deadzone(), 0.6); // not a number: keep the previous value
    nav.setDeadzone(0.4);
    QCOMPARE(nav.deadzone(), 0.4);
}

void TestGamepadNavigator::repeatedPressWithoutReleaseActsOnce()
{
    // An OS/kernel key-autorepeat that re-reports a held button as "pressed" must not act
    // again for Confirm/Back and friends; only navigation repeats (and does so on its own
    // timer, see heldDirectionRepeats()).
    GamepadNavigator nav;
    nav.setRepeatDelays(10s, 10s);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);
    QSignalSpy raw(&nav, &GamepadNavigator::buttonPressed);
    nav.buttonEvent(GamepadButton::South, true);
    nav.buttonEvent(GamepadButton::South, true);
    nav.buttonEvent(GamepadButton::South, true);
    QCOMPARE(actions(spy), QList<NavAction>{NavAction::Accept});
    QCOMPARE(raw.count(), 1);

    nav.buttonEvent(GamepadButton::South, false);
    nav.buttonEvent(GamepadButton::South, true); // a fresh physical press acts again
    QCOMPARE(spy.count(), 2);

    nav.buttonEvent(GamepadButton::East, true);
    nav.buttonEvent(GamepadButton::East, true);
    QCOMPARE(spy.count(), 3);

    // Directions are unaffected: each repeat still navigates.
    spy.clear();
    nav.buttonEvent(GamepadButton::DpadDown, true);
    nav.buttonEvent(GamepadButton::DpadDown, true);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Down, NavAction::Down}));
}

void TestGamepadNavigator::rawEventsAreReportedBeforeThresholds()
{
    GamepadNavigator nav;
    nav.setDeadzone(0.25);
    QSignalSpy buttons(&nav, &GamepadNavigator::rawButtonEvent);
    QSignalSpy axes(&nav, &GamepadNavigator::rawAxisEvent);
    QSignalSpy spy(&nav, &GamepadNavigator::actionTriggered);

    nav.buttonEvent(GamepadButton::South, true, 7);
    nav.buttonEvent(GamepadButton::South, true, 7); // deduped for actions, still raw
    nav.buttonEvent(GamepadButton::South, false, 7);
    QCOMPARE(buttons.count(), 3);
    QCOMPARE(buttons.at(0).at(0).value<GamepadButton>(), GamepadButton::South);
    QCOMPARE(buttons.at(0).at(1).toBool(), true);
    QCOMPARE(buttons.at(0).at(2).toUInt(), 7u);
    QCOMPARE(buttons.at(2).at(1).toBool(), false);

    nav.axisEvent(GamepadAxis::LeftX, -0.1, 7); // inside the dead zone: no action
    nav.axisEvent(GamepadAxis::LeftX, -0.82, 7);
    QCOMPARE(axes.count(), 2);
    QCOMPARE(axes.at(0).at(0).value<GamepadAxis>(), GamepadAxis::LeftX);
    QCOMPARE(axes.at(0).at(1).toDouble(), -0.1);
    QCOMPARE(axes.at(1).at(1).toDouble(), -0.82);
    QCOMPARE(actions(spy), (QList<NavAction>{NavAction::Accept, NavAction::Left}));

    nav.axisEvent(GamepadAxis::LeftX, std::nan(""), 7); // never reported, never acted on
    QCOMPARE(axes.count(), 2);
}

QTEST_GUILESS_MAIN(TestGamepadNavigator)
#include "tst_gamepadnavigator.moc"
