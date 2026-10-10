#include "nexora/system/UnixSignalWatcher.h"

#include "nexora/shared/Logging.h"

#include <QSocketNotifier>

#include <csignal>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nexora::system {

namespace {

int g_pipe[2] = {-1, -1};

void handler(int sig)
{
    const unsigned char byte = static_cast<unsigned char>(sig);
    // Async-signal-safe; if the pipe is full the signal is already pending anyway.
    [[maybe_unused]] const auto ignored = ::write(g_pipe[1], &byte, 1);
}

constexpr int kSignals[] = {SIGTERM, SIGINT, SIGHUP};

} // namespace

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent)
{
    if (g_pipe[0] >= 0) {
        qCWarning(lcSystem) << "UnixSignalWatcher already active";
        return;
    }
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, g_pipe) != 0) {
        qCWarning(lcSystem) << "Cannot create signal pipe; termination signals use default handling";
        g_pipe[0] = g_pipe[1] = -1;
        return;
    }
    struct sigaction sa {};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    for (int sig : kSignals)
        ::sigaction(sig, &sa, nullptr);
    m_notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &UnixSignalWatcher::onReadable);
}

UnixSignalWatcher::~UnixSignalWatcher()
{
    if (!m_notifier)
        return;
    struct sigaction sa {};
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    for (int sig : kSignals)
        ::sigaction(sig, &sa, nullptr);
    delete m_notifier;
    m_notifier = nullptr;
    ::close(g_pipe[0]);
    ::close(g_pipe[1]);
    g_pipe[0] = g_pipe[1] = -1;
}

void UnixSignalWatcher::onReadable()
{
    unsigned char byte = 0;
    while (::read(g_pipe[0], &byte, 1) == 1) {
        qCInfo(lcSystem) << "Received signal" << int(byte);
        emit signalReceived(int(byte));
    }
}

} // namespace nexora::system
