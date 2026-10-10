#include "nexora/system/SingleInstanceGuard.h"

#include <QDir>
#include <QStandardPaths>

namespace nexora::system {

SingleInstanceGuard::SingleInstanceGuard(QString lockPath)
    : m_path(std::move(lockPath))
    , m_lock(std::make_unique<QLockFile>(m_path))
{
    m_lock->setStaleLockTime(0); // only a dead owner makes a lock stale, never its age
}

QString SingleInstanceGuard::defaultLockPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return dir + QStringLiteral("/nexora-console-ui.lock");
}

bool SingleInstanceGuard::tryAcquire()
{
    return m_lock->tryLock(0);
}

qint64 SingleInstanceGuard::ownerPid() const
{
    qint64 pid = 0;
    QString host;
    QString app;
    return m_lock->getLockInfo(&pid, &host, &app) ? pid : 0;
}

} // namespace nexora::system
