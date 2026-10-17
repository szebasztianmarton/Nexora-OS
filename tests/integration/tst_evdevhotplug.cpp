// Plugs virtual controllers (Linux uinput) into a running EvdevGamepadBackend, one per family
// of docs/linux_gamepad_drivers.md (profiles in tests/fixtures/virtual-pad/VirtualPad.h, the
// same ones nexora-virtual-pad offers). For every profile and every face/shoulder/centre
// button, the key code the real driver sends must arrive at the right position; each pad
// must be picked up by hotplug and dropped again when unplugged. Devices that are not
// controllers (keyboard, motion sensor) must be ignored.
//
// Needs write access to /dev/uinput and read access to the /dev/input/event* nodes it
// creates (CI: modprobe uinput plus a udev rule, see .github/workflows/ci.yml); skipped
// otherwise. uinput devices have no kernel driver in sysfs, so the layout is chosen from the
// vendor id exactly as for a Bluetooth pad on hid-generic.
#include "VirtualPad.h"
#include "input/EvdevGamepadBackend.h"
#include "input/GamepadNavigator.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <utility>

using nexora::input::EvdevGamepadBackend;
using nexora::input::GamepadAxis;
using nexora::input::GamepadButton;
using nexora::input::GamepadNavigator;
using nexora::testing::VirtualPad;

namespace {

constexpr int Timeout = 5000;

bool uinputAvailable()
{
    const int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;
    ::close(fd);
    return true;
}

bool hasDevice(const EvdevGamepadBackend &backend, const QString &name)
{
    return backend.deviceNames().contains(name);
}

} // namespace

class TestEvdevHotplug : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!uinputAvailable())
            QSKIP("/dev/uinput is not writable (modprobe uinput and grant access to run this test)");
    }

    void controllerIsHotpluggedAndMapped_data()
    {
        QTest::addColumn<QString>("profileId");
        for (const auto &profile : nexora::testing::padProfiles())
            QTest::newRow(profile.id) << QString::fromLatin1(profile.id);
    }

    void controllerIsHotpluggedAndMapped()
    {
        QFETCH(QString, profileId);
        const auto *profile = nexora::testing::findPadProfile(profileId.toStdString());
        QVERIFY(profile);
        const QString name = QStringLiteral("Nexora Test %1").arg(profileId);

        GamepadNavigator navigator;
        EvdevGamepadBackend backend(&navigator);
        QVERIFY(backend.start());
        QVERIFY(!hasDevice(backend, name));
        QSignalSpy buttons(&navigator, &GamepadNavigator::rawButtonEvent);
        QSignalSpy axes(&navigator, &GamepadNavigator::rawAxisEvent);

        VirtualPad pad(*profile, name.toStdString()); // plugged in after the backend started
        QVERIFY2(pad.isValid(), pad.error().c_str());
        QTRY_VERIFY_WITH_TIMEOUT(hasDevice(backend, name), Timeout);

        const std::pair<const char *, GamepadButton> positions[] = {
            {"south", GamepadButton::South},   {"east", GamepadButton::East},   {"west", GamepadButton::West},
            {"north", GamepadButton::North},   {"l1", GamepadButton::L1},       {"r1", GamepadButton::R1},
            {"select", GamepadButton::Select}, {"start", GamepadButton::Start}, {"guide", GamepadButton::Guide},
        };
        for (const auto &[position, expected] : positions) {
            const int code = nexora::testing::padButtonCode(profile->family, position);
            if (code < 0)
                continue; // e.g. no guide button on joystick-class pads
            buttons.clear();
            QVERIFY(pad.send(EV_KEY, code, 1));
            QVERIFY(pad.send(EV_KEY, code, 0));
            QTRY_COMPARE_WITH_TIMEOUT(buttons.size(), 2, Timeout);
            QCOMPARE(buttons.at(0).at(0).value<GamepadButton>(), expected);
            QCOMPARE(buttons.at(0).at(1).toBool(), true);
            QCOMPARE(buttons.at(1).at(1).toBool(), false);
        }

        // The D-pad hat navigates on every family.
        QVERIFY(pad.send(EV_ABS, ABS_HAT0X, -1));
        QTRY_VERIFY_WITH_TIMEOUT(!axes.isEmpty(), Timeout);
        QCOMPARE(axes.last().at(0).value<GamepadAxis>(), GamepadAxis::HatX);
        QCOMPARE(axes.last().at(1).toDouble(), -1.0);

        pad.unplug();
        QTRY_VERIFY_WITH_TIMEOUT(!hasDevice(backend, name), Timeout);
    }

    // Two pads at once, then replugging the first one.
    void severalControllersAndReplug()
    {
        GamepadNavigator navigator;
        EvdevGamepadBackend backend(&navigator);
        QVERIFY(backend.start());
        const auto *xbox = nexora::testing::findPadProfile("xbox360");
        const auto *dualSense = nexora::testing::findPadProfile("dualsense");
        const QString nameA = QStringLiteral("Nexora Test Pad A");
        const QString nameB = QStringLiteral("Nexora Test Pad B");
        auto first = std::make_unique<VirtualPad>(*xbox, nameA.toStdString());
        VirtualPad second(*dualSense, nameB.toStdString());
        QVERIFY(first->isValid() && second.isValid());
        QTRY_VERIFY_WITH_TIMEOUT(hasDevice(backend, nameA) && hasDevice(backend, nameB), Timeout);
        first.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!hasDevice(backend, nameA), Timeout);
        QVERIFY(hasDevice(backend, nameB));
        first = std::make_unique<VirtualPad>(*xbox, nameA.toStdString());
        QVERIFY(first->isValid());
        QTRY_VERIFY_WITH_TIMEOUT(hasDevice(backend, nameA), Timeout);
    }

    // A keyboard and a controller's motion sensor node are never treated as controllers.
    void otherDevicesAreIgnored()
    {
        GamepadNavigator navigator;
        EvdevGamepadBackend backend(&navigator);
        QVERIFY(backend.start());
        VirtualPad keyboard("Nexora Test Keyboard", 0x1234, 0x0001, {KEY_A, KEY_B, KEY_ENTER}, {});
        VirtualPad motion("Nexora Test Motion Sensors", 0x054c, 0x0ce6, {}, {ABS_X, ABS_Y, ABS_Z}, true);
        VirtualPad marker(*nexora::testing::findPadProfile("dualsense"), "Nexora Test Marker");
        QVERIFY(keyboard.isValid() && motion.isValid() && marker.isValid());
        // Once the marker pad (plugged in last) is there, the others were probed too.
        QTRY_VERIFY_WITH_TIMEOUT(hasDevice(backend, QStringLiteral("Nexora Test Marker")), Timeout);
        QVERIFY(!hasDevice(backend, QStringLiteral("Nexora Test Keyboard")));
        QVERIFY(!hasDevice(backend, QStringLiteral("Nexora Test Motion Sensors")));
    }
};

QTEST_GUILESS_MAIN(TestEvdevHotplug)
#include "tst_evdevhotplug.moc"
