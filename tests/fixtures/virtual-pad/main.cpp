// nexora-virtual-pad: plugs a virtual game controller into this Linux machine (uinput), for
// trying controller support without the hardware: on a development PC next to the console
// UI, or passed into a QEMU test VM (virtio-input-host-pci, see docs/controller-support.md).
//
//   nexora-virtual-pad --list
//   nexora-virtual-pad [--profile ID] [--name NAME]
//
// The pad stays plugged in until stdin closes or "quit". Commands on stdin, one per line:
//   south east west north l1 r1 select start guide   press and release that button
//   up down left right                                 D-pad (hat) press and release
//   hold BUTTON / release BUTTON                       for chords (e.g. select + start)
//   unplug / plug                                      pull the cable / plug it in again
//   quit
//
// Needs write access to /dev/uinput (e.g. sudo modprobe uinput; sudo chmod 0666 /dev/uinput).
#include "VirtualPad.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

using nexora::testing::PadProfile;
using nexora::testing::VirtualPad;

namespace {

constexpr auto kPressTime = std::chrono::milliseconds(80);

void usage(std::ostream &out)
{
    out << "usage: nexora-virtual-pad --list\n"
           "       nexora-virtual-pad [--profile ID] [--name NAME]\n"
           "Commands on stdin: south east west north l1 r1 select start guide, up down left right,\n"
           "hold BUTTON, release BUTTON, unplug, plug, quit\n";
}

std::unique_ptr<VirtualPad> plug(const PadProfile &profile, const std::string &name)
{
    auto pad = std::make_unique<VirtualPad>(profile, name);
    if (!pad->isValid()) {
        std::cerr << "nexora-virtual-pad: " << pad->error() << '\n';
        return nullptr;
    }
    // Give udev a moment to create the node before the first input.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::cout << "plugged in: " << (name.empty() ? profile.name : name) << " (" << profile.id
              << ", /sys/devices/virtual/input/" << pad->sysName() << ")" << std::endl;
    return pad;
}

bool direction(VirtualPad &pad, const std::string &word)
{
    const bool vertical = word == "up" || word == "down";
    if (!vertical && word != "left" && word != "right")
        return false;
    const int axis = vertical ? ABS_HAT0Y : ABS_HAT0X;
    const int value = word == "up" || word == "left" ? -1 : 1;
    pad.send(EV_ABS, axis, value);
    std::this_thread::sleep_for(kPressTime);
    pad.send(EV_ABS, axis, 0);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    const PadProfile *profile = nexora::testing::findPadProfile("xbox360");
    std::string name;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            for (const PadProfile &p : nexora::testing::padProfiles()) {
                std::cout << p.id << '\t' << std::hex << std::setfill('0') << std::setw(4) << p.vendor << ':'
                          << std::setw(4) << p.product << std::dec << '\t' << p.name << '\n';
            }
            return 0;
        }
        if (arg == "--profile" && i + 1 < argc) {
            profile = nexora::testing::findPadProfile(argv[++i]);
            if (!profile) {
                std::cerr << "nexora-virtual-pad: unknown profile '" << argv[i] << "' (see --list)\n";
                return 2;
            }
        } else if (arg == "--name" && i + 1 < argc) {
            name = argv[++i];
        } else {
            usage(arg == "--help" || arg == "-h" ? std::cout : std::cerr);
            return arg == "--help" || arg == "-h" ? 0 : 2;
        }
    }

    auto pad = plug(*profile, name);
    if (!pad)
        return 1;

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream words(line);
        std::string command;
        std::string argument;
        words >> command >> argument;
        if (command.empty())
            continue;
        if (command == "quit")
            break;
        if (command == "unplug") {
            pad.reset();
            std::cout << "unplugged" << std::endl;
            continue;
        }
        if (command == "plug") {
            if (!pad)
                pad = plug(*profile, name);
            if (!pad)
                return 1;
            continue;
        }
        if (!pad) {
            std::cerr << "unplugged; type 'plug' first\n";
            continue;
        }
        if (direction(*pad, command))
            continue;
        const bool hold = command == "hold";
        const bool release = command == "release";
        const std::string button = hold || release ? argument : command;
        const int code = nexora::testing::padButtonCode(profile->family, button);
        if (code < 0) {
            std::cerr << "unknown or unavailable button '" << button << "' for " << profile->id << '\n';
            continue;
        }
        if (!release)
            pad->send(EV_KEY, code, 1);
        if (!hold && !release)
            std::this_thread::sleep_for(kPressTime);
        if (!hold)
            pad->send(EV_KEY, code, 0);
    }
    return 0;
}
