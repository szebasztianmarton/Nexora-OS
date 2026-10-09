// nexora-fake-emulator: simulates an emulator process for automated tests.
//
// Options are applied in this order, regardless of their position on the command line:
//   --startup-delay-ms N   sleep before doing anything (slow startup)
//   --ignore-sigterm       ignore SIGTERM (forces the manager to escalate to SIGKILL)
//   (prints "ready" on stdout once signal handling is configured)
//   --stdout TEXT          write TEXT + newline to stdout (repeatable)
//   --stderr TEXT          write TEXT + newline to stderr (repeatable)
//   --sleep-ms N           stay alive for N ms (capped at 10 minutes)
//   --crash segv|abort     terminate abnormally via SIGSEGV or SIGABRT (no core dump)
//   --exit-code N          exit with status N (default 0)
// Unknown options exit with status 64.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/resource.h>

namespace {

constexpr long MaxSleepMs = 10L * 60L * 1000L;

void sleepMs(long ms)
{
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(ms, MaxSleepMs)));
}

[[noreturn]] void usageError(std::string_view message)
{
    std::fprintf(stderr, "nexora-fake-emulator: %.*s\n", int(message.size()), message.data());
    std::exit(64);
}

long parseNumber(std::string_view option, const char *value)
{
    char *end = nullptr;
    const long result = std::strtol(value, &end, 10);
    if (end == value || *end != '\0')
        usageError(std::string("invalid number for ") + std::string(option));
    return result;
}

[[noreturn]] void crash(std::string_view mode)
{
    // Keep CI hosts clean: no core files from intentional crashes.
    const rlimit noCore{0, 0};
    ::setrlimit(RLIMIT_CORE, &noCore);
    std::fflush(nullptr);
    if (mode == "abort") {
        std::signal(SIGABRT, SIG_DFL);
        std::abort();
    }
    std::signal(SIGSEGV, SIG_DFL);
    std::raise(SIGSEGV);
    std::_Exit(139); // unreachable in practice
}

} // namespace

int main(int argc, char **argv)
{
    long startupDelayMs = 0;
    long sleepForMs = 0;
    int exitCode = 0;
    bool ignoreSigterm = false;
    std::string crashMode;
    std::vector<std::string> stdoutLines;
    std::vector<std::string> stderrLines;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto value = [&]() -> const char * {
            if (i + 1 >= argc)
                usageError(std::string("missing value for ") + std::string(arg));
            return argv[++i];
        };
        if (arg == "--startup-delay-ms")
            startupDelayMs = parseNumber(arg, value());
        else if (arg == "--sleep-ms")
            sleepForMs = parseNumber(arg, value());
        else if (arg == "--exit-code")
            exitCode = int(parseNumber(arg, value()));
        else if (arg == "--stdout")
            stdoutLines.emplace_back(value());
        else if (arg == "--stderr")
            stderrLines.emplace_back(value());
        else if (arg == "--ignore-sigterm")
            ignoreSigterm = true;
        else if (arg == "--crash") {
            crashMode = value();
            if (crashMode != "segv" && crashMode != "abort")
                usageError("--crash expects segv or abort");
        } else
            usageError(std::string("unknown option ") + std::string(arg));
    }

    sleepMs(startupDelayMs);
    if (ignoreSigterm)
        std::signal(SIGTERM, SIG_IGN);

    std::fputs("ready\n", stdout);
    std::fflush(stdout);

    for (const auto &line : stdoutLines)
        std::fprintf(stdout, "%s\n", line.c_str());
    std::fflush(stdout);
    for (const auto &line : stderrLines)
        std::fprintf(stderr, "%s\n", line.c_str());
    std::fflush(stderr);

    sleepMs(sleepForMs);

    if (!crashMode.empty())
        crash(crashMode);
    return exitCode;
}
