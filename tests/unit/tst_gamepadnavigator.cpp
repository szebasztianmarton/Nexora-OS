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
    void resetDeviceDropsOnlyThatDevice();
#ifdef NEXORA_HAVE_EVDEV
    void evdevKeyMapping();
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

QTEST_GUILESS_MAIN(TestGamepadNavigator)
#include "tst_gamepadnavigator.moc"
