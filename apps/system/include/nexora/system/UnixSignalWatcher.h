#pragma once

#include <QObject>

class QSocketNotifier;

namespace nexora::system {

// Turns SIGTERM / SIGINT / SIGHUP into a Qt signal on the main thread (self-pipe), so the
// application can stop its emulator and save state instead of dying mid-write.
// Only one instance may exist at a time.
class UnixSignalWatcher : public QObject
{
    Q_OBJECT

public:
    explicit UnixSignalWatcher(QObject *parent = nullptr);
    ~UnixSignalWatcher() override;

    bool isActive() const { return m_notifier != nullptr; }

signals:
    void signalReceived(int signalNumber);

private:
    void onReadable();

    QSocketNotifier *m_notifier = nullptr;
};

} // namespace nexora::system
