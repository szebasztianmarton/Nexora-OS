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
//   --print-cwd            print "cwd: <dir>" after "ready"
//   --echo-args            print "arg: <value>" for every argument after "ready"
//   --spawn-child          fork a child (a "grandchild" of the launcher) that inherits the
//                          signal setup and sleeps for --sleep-ms; prints "child: <pid>"
//   --load-state FILE      read a state written by SAVE below and print
//                          "loaded-state: <its first line>" after "ready"; exit 65 if unreadable
//   --control-socket PATH  connect to Nexora's save-state control socket after "ready" and
//                          answer it while sleeping (docs/emulator-integration.md#capabilities):
//                            SAVE <dir> -> writes <dir>/fake-<pid>-<n>.state, "OK <file>"
//                            QUIT       -> "OK", then exits with --exit-code
//   --save-error TEXT      answer SAVE with "ERR TEXT" instead
//   --save-empty           write an empty state file (the adapter must reject it)
//   --save-delay-ms N      wait N ms before answering SAVE
//   --ignore-quit          answer QUIT with "ERR busy" and keep running
// Arguments that are not options (e.g. a game file path) are accepted and ignored.
// Unknown options (anything else starting with "--") exit with status 64.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

struct ControlOptions
{
    std::string socketPath;
    std::string saveError;
    bool saveEmpty = false;
    long saveDelayMs = 0;
    bool ignoreQuit = false;
    long saves = 0; // continues from a loaded state
};

void sendLine(int fd, const std::string &line)
{
    const std::string out = line + "\n";
    (void)::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
}

int connectControlSocket(const std::string &path)
{
    sockaddr_un address{};
    if (path.size() >= sizeof(address.sun_path))
        usageError("control socket path too long");
    address.sun_family = AF_UNIX;
    path.copy(address.sun_path, path.size());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0 || ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        std::fprintf(stderr, "nexora-fake-emulator: cannot connect to %s\n", path.c_str());
        if (fd >= 0)
            ::close(fd);
        return -1;
    }
    return fd;
}

// Handles one protocol line; returns true when the process should exit.
bool handleLine(int fd, const std::string &line, ControlOptions &options)
{
    if (line.rfind("SAVE ", 0) == 0) {
        sleepMs(options.saveDelayMs);
        if (!options.saveError.empty()) {
            sendLine(fd, "ERR " + options.saveError);
            return false;
        }
        ++options.saves;
        const std::string file
            = line.substr(5) + "/fake-" + std::to_string(::getpid()) + "-" + std::to_string(options.saves) + ".state";
        std::FILE *out = std::fopen(file.c_str(), "wx");
        if (!out) {
            sendLine(fd, "ERR cannot write " + file);
            return false;
        }
        if (!options.saveEmpty)
            std::fprintf(out, "fake-state %ld\n", options.saves);
        std::fclose(out);
        std::fprintf(stdout, "saved-state: %s\n", file.c_str());
        std::fflush(stdout);
        sendLine(fd, "OK " + file);
        return false;
    }
    if (line == "QUIT") {
        if (options.ignoreQuit) {
            sendLine(fd, "ERR busy");
            return false;
        }
        sendLine(fd, "OK");
        return true;
    }
    sendLine(fd, "ERR unknown command");
    return false;
}

// Sleeps for |ms| while answering the control socket. Returns true if QUIT ended it.
bool serve(long ms, ControlOptions &options)
{
    const int fd = connectControlSocket(options.socketPath);
    if (fd < 0) {
        sleepMs(ms);
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::min(ms, MaxSleepMs));
    std::string buffer;
    bool open = true;
    while (true) {
        const auto left
            = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
                  .count();
        if (left <= 0)
            break;
        if (!open) {
            sleepMs(left);
            break;
        }
        pollfd p{fd, POLLIN, 0};
        if (::poll(&p, 1, int(std::min<long long>(left, 1000))) <= 0)
            continue;
        char chunk[512];
        const ssize_t n = ::read(fd, chunk, sizeof(chunk));
        if (n <= 0) {
            open = false; // Nexora closed the socket: keep running like a real emulator
            continue;
        }
        buffer.append(chunk, size_t(n));
        for (size_t newline = buffer.find('\n'); newline != std::string::npos; newline = buffer.find('\n')) {
            const std::string line = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (handleLine(fd, line, options)) {
                ::close(fd);
                return true;
            }
        }
    }
    ::close(fd);
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    long startupDelayMs = 0;
    long sleepForMs = 0;
    int exitCode = 0;
    bool ignoreSigterm = false;
    bool printCwd = false;
    bool echoArgs = false;
    bool spawnChild = false;
    std::string crashMode;
    std::string loadState;
    ControlOptions control;
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
        else if (arg == "--print-cwd")
            printCwd = true;
        else if (arg == "--echo-args")
            echoArgs = true;
        else if (arg == "--spawn-child")
            spawnChild = true;
        else if (arg == "--load-state")
            loadState = value();
        else if (arg == "--control-socket")
            control.socketPath = value();
        else if (arg == "--save-error")
            control.saveError = value();
        else if (arg == "--save-empty")
            control.saveEmpty = true;
        else if (arg == "--save-delay-ms")
            control.saveDelayMs = parseNumber(arg, value());
        else if (arg == "--ignore-quit")
            control.ignoreQuit = true;
        else if (arg == "--crash") {
            crashMode = value();
            if (crashMode != "segv" && crashMode != "abort")
                usageError("--crash expects segv or abort");
        } else if (arg.rfind("--", 0) == 0)
            usageError(std::string("unknown option ") + std::string(arg));
    }

    sleepMs(startupDelayMs);
    if (ignoreSigterm)
        std::signal(SIGTERM, SIG_IGN);

    std::fputs("ready\n", stdout);
    if (printCwd) {
        char cwd[4096];
        if (::getcwd(cwd, sizeof(cwd)))
            std::fprintf(stdout, "cwd: %s\n", cwd);
    }
    if (echoArgs) {
        for (int i = 1; i < argc; ++i)
            std::fprintf(stdout, "arg: %s\n", argv[i]);
    }
    if (!loadState.empty()) {
        std::FILE *in = std::fopen(loadState.c_str(), "r");
        char line[256] = {};
        if (!in || !std::fgets(line, sizeof(line), in)) {
            std::fprintf(stderr, "nexora-fake-emulator: cannot read state %s\n", loadState.c_str());
            std::exit(65);
        }
        std::fclose(in);
        std::string text(line);
        if (!text.empty() && text.back() == '\n')
            text.pop_back();
        std::fprintf(stdout, "loaded-state: %s\n", text.c_str());
        if (text.rfind("fake-state ", 0) == 0)
            control.saves = std::strtol(text.c_str() + 11, nullptr, 10);
    }
    std::fflush(stdout);
    if (spawnChild) {
        const pid_t child = ::fork();
        if (child == 0) {
            sleepMs(sleepForMs);
            std::_Exit(0);
        }
        if (child > 0)
            std::fprintf(stdout, "child: %ld\n", long(child));
        std::fflush(stdout);
    }

    for (const auto &line : stdoutLines)
        std::fprintf(stdout, "%s\n", line.c_str());
    std::fflush(stdout);
    for (const auto &line : stderrLines)
        std::fprintf(stderr, "%s\n", line.c_str());
    std::fflush(stderr);

    if (!control.socketPath.empty()) {
        if (serve(sleepForMs, control))
            return exitCode;
    } else {
        sleepMs(sleepForMs);
    }

    if (!crashMode.empty())
        crash(crashMode);
    return exitCode;
}
