#pragma once

#include <QLockFile>
#include <QString>

#include <memory>

namespace nexora::system {

// Prevents two Nexora UIs from running for the same user (they would fight over the
// display, controllers and the library database). Stale locks of crashed instances are
// detected by QLockFile (PID + hostname check).
class SingleInstanceGuard
{
public:
    // Lock file: $XDG_RUNTIME_DIR/nexora-console-ui.lock (falls back to the temp dir).
    explicit SingleInstanceGuard(QString lockPath = defaultLockPath());

    bool tryAcquire();
    // PID of the instance holding the lock, or 0.
    qint64 ownerPid() const;
    QString lockPath() const { return m_path; }

    static QString defaultLockPath();

private:
    QString m_path;
    std::unique_ptr<QLockFile> m_lock;
};

} // namespace nexora::system
