#pragma once
// Virtual game controllers through Linux uinput, shared by tst_evdevhotplug and the
// nexora-virtual-pad tool. Each profile reproduces one family of
// docs/linux_gamepad_drivers.md: its vendor/product ids and the key codes its kernel driver
// reports (drivers/input/joystick/xpad.c, drivers/hid/hid-playstation.c, hid-nintendo.c,
// hid-steam.c, and hid-input.c for generic HID pads and joysticks). Header-only, no Qt.

#include <linux/input.h>
#include <linux/uinput.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace nexora::testing {

// How the real driver reports buttons (see EvdevLayout in the console UI).
enum class PadFamily {
    XboxLabels, // BTN_A/B/X/Y by Xbox label: X (left) is BTN_X == BTN_NORTH (xpad, hid-steam)
    Positional, // BTN_SOUTH/EAST/NORTH/WEST by position (hid-playstation, hid-nintendo, ...)
    JoystickPc, // numbered BTN_TRIGGER... buttons, Logitech Dual Action order
    JoystickPlayStation, // numbered buttons, DragonRise "USB Gamepad" order
};

struct PadProfile
{
    const char *id; // command-line name
    const char *name; // device name as the driver reports it
    std::uint16_t vendor;
    std::uint16_t product;
    PadFamily family;
};

inline const std::array<PadProfile, 10> &padProfiles()
{
    static const std::array<PadProfile, 10> profiles{{
        {"xbox360", "Microsoft X-Box 360 pad", 0x045e, 0x028e, PadFamily::XboxLabels},
        {"xbox-series", "Microsoft Xbox Series S|X Controller", 0x045e, 0x0b12, PadFamily::XboxLabels},
        {"steam", "Valve Software Steam Controller", 0x28de, 0x1102, PadFamily::XboxLabels},
        {"dualsense", "Sony Interactive Entertainment DualSense Wireless Controller", 0x054c, 0x0ce6,
         PadFamily::Positional},
        {"dualshock4", "Sony Interactive Entertainment Wireless Controller", 0x054c, 0x09cc, PadFamily::Positional},
        {"switch-pro", "Nintendo Switch Pro Controller", 0x057e, 0x2009, PadFamily::Positional},
        {"8bitdo-pro2", "8BitDo Pro 2", 0x2dc8, 0x6003, PadFamily::Positional},
        {"dual-action", "Logitech Logitech Dual Action", 0x046d, 0xc216, PadFamily::JoystickPc},
        {"dragonrise", "DragonRise Inc. Generic USB Joystick", 0x0079, 0x0006, PadFamily::JoystickPlayStation},
        {"wheel", "Thrustmaster T150 Racing Wheel", 0x044f, 0xb677, PadFamily::JoystickPc},
    }};
    return profiles;
}

inline const PadProfile *findPadProfile(std::string_view id)
{
    for (const PadProfile &profile : padProfiles()) {
        if (id == profile.id)
            return &profile;
    }
    return nullptr;
}

inline std::vector<int> padKeys(PadFamily family)
{
    switch (family) {
    case PadFamily::XboxLabels:
        return {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};
    case PadFamily::Positional:
        return {BTN_SOUTH,  BTN_EAST,  BTN_NORTH, BTN_WEST,   BTN_TL,    BTN_TR,
                BTN_SELECT, BTN_START, BTN_MODE,  BTN_THUMBL, BTN_THUMBR};
    case PadFamily::JoystickPc:
    case PadFamily::JoystickPlayStation:
        return {BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_TOP,   BTN_TOP2,  BTN_PINKIE,
                BTN_BASE,    BTN_BASE2, BTN_BASE3,  BTN_BASE4, BTN_BASE5, BTN_BASE6};
    }
    return {};
}

inline std::vector<int> padAxes(PadFamily family)
{
    if (family == PadFamily::JoystickPc || family == PadFamily::JoystickPlayStation)
        return {ABS_X, ABS_Y, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
    return {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
}

// Key code the real driver sends for the button at |position| ("south", "east", "west",
// "north", "start", "select", "guide", "l1", "r1"); -1 if the family has no such button.
inline int padButtonCode(PadFamily family, std::string_view position)
{
    struct Entry
    {
        std::string_view position;
        int xbox, positional, pc, playStation;
    };
    static constexpr std::array<Entry, 9> table{{
        {"south", BTN_A, BTN_SOUTH, BTN_THUMB, BTN_THUMB2},
        {"east", BTN_B, BTN_EAST, BTN_THUMB2, BTN_THUMB},
        {"west", BTN_X, BTN_WEST, BTN_TRIGGER, BTN_TOP},
        {"north", BTN_Y, BTN_NORTH, BTN_TOP, BTN_TRIGGER},
        {"l1", BTN_TL, BTN_TL, BTN_TOP2, BTN_TOP2},
        {"r1", BTN_TR, BTN_TR, BTN_PINKIE, BTN_PINKIE},
        {"select", BTN_SELECT, BTN_SELECT, BTN_BASE3, BTN_BASE3},
        {"start", BTN_START, BTN_START, BTN_BASE4, BTN_BASE4},
        {"guide", BTN_MODE, BTN_MODE, -1, -1},
    }};
    for (const Entry &entry : table) {
        if (entry.position != position)
            continue;
        switch (family) {
        case PadFamily::XboxLabels: return entry.xbox;
        case PadFamily::Positional: return entry.positional;
        case PadFamily::JoystickPc: return entry.pc;
        case PadFamily::JoystickPlayStation: return entry.playStation;
        }
    }
    return -1;
}

// A virtual input device; destroying it (or unplug()) removes it like pulling the cable.
class VirtualPad
{
public:
    VirtualPad(const std::string &name, std::uint16_t vendor, std::uint16_t product, const std::vector<int> &keys,
               const std::vector<int> &axes, bool accelerometer = false)
    {
        m_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
        if (m_fd < 0) {
            m_error = std::string("/dev/uinput: ") + std::strerror(errno);
            return;
        }
        bool ok = true;
        if (!keys.empty()) {
            ok = ok && ::ioctl(m_fd, UI_SET_EVBIT, EV_KEY) == 0;
            for (const int key : keys)
                ok = ok && ::ioctl(m_fd, UI_SET_KEYBIT, key) == 0;
        }
        if (!axes.empty()) {
            ok = ok && ::ioctl(m_fd, UI_SET_EVBIT, EV_ABS) == 0;
            for (const int axis : axes) {
                uinput_abs_setup abs{};
                abs.code = static_cast<__u16>(axis);
                const bool hat = axis == ABS_HAT0X || axis == ABS_HAT0Y;
                abs.absinfo.minimum = hat ? -1 : -32768;
                abs.absinfo.maximum = hat ? 1 : 32767;
                ok = ok && ::ioctl(m_fd, UI_SET_ABSBIT, axis) == 0 && ::ioctl(m_fd, UI_ABS_SETUP, &abs) == 0;
            }
        }
        if (accelerometer)
            ok = ok && ::ioctl(m_fd, UI_SET_PROPBIT, INPUT_PROP_ACCELEROMETER) == 0;

        uinput_setup setup{};
        setup.id.bustype = BUS_USB;
        setup.id.vendor = vendor;
        setup.id.product = product;
        std::strncpy(setup.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
        ok = ok && ::ioctl(m_fd, UI_DEV_SETUP, &setup) == 0 && ::ioctl(m_fd, UI_DEV_CREATE) == 0;
        if (!ok) {
            m_error = std::string("uinput device setup failed: ") + std::strerror(errno);
            ::close(m_fd);
            m_fd = -1;
        }
    }

    // |name| overrides the profile's device name (tests use unique names).
    explicit VirtualPad(const PadProfile &profile, const std::string &name = {})
        : VirtualPad(name.empty() ? std::string(profile.name) : name, profile.vendor, profile.product,
                     padKeys(profile.family), padAxes(profile.family))
    { }

    ~VirtualPad() { unplug(); }
    VirtualPad(const VirtualPad &) = delete;
    VirtualPad &operator=(const VirtualPad &) = delete;

    bool isValid() const { return m_fd >= 0; }
    const std::string &error() const { return m_error; }

    // sysfs name of the input device ("input42"), empty if unknown.
    std::string sysName() const
    {
        char name[64] = {};
        if (m_fd < 0 || ::ioctl(m_fd, UI_GET_SYSNAME(sizeof(name)), name) < 0)
            return {};
        return name;
    }

    void unplug()
    {
        if (m_fd < 0)
            return;
        ::ioctl(m_fd, UI_DEV_DESTROY);
        ::close(m_fd);
        m_fd = -1;
    }

    // Sends one event followed by SYN_REPORT.
    bool send(int type, int code, int value)
    {
        input_event events[2]{};
        events[0].type = static_cast<__u16>(type);
        events[0].code = static_cast<__u16>(code);
        events[0].value = value;
        events[1].type = EV_SYN;
        events[1].code = SYN_REPORT;
        return m_fd >= 0 && ::write(m_fd, events, sizeof(events)) == static_cast<ssize_t>(sizeof(events));
    }

private:
    int m_fd = -1;
    std::string m_error;
};

} // namespace nexora::testing
