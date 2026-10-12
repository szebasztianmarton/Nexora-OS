#include "UpdateController.h"

#include "nexora/update/UpdateService.h"

#include <QDir>
#include <QFileInfo>

namespace nexora::ui {

using update::UpdateService;
using update::UpdateState;

namespace {

// Expands one absolute glob pattern ("/media/*/nexora-update-*") against the real
// filesystem, one path component at a time. Wildcarded components only ever match
// directories; symlinks are never followed (neither as an intermediate component nor as
// the final match), so a crafted removable-media symlink cannot point this outside the
// locations it is meant to scan.
QStringList expandDirectoryGlob(const QString &pattern)
{
    const QStringList parts = pattern.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList current{QStringLiteral("/")};
    for (const QString &part : parts) {
        QStringList next;
        for (const QString &base : current) {
            QDir dir(base);
            if (!dir.exists())
                continue;
            if (part.contains(QLatin1Char('*')) || part.contains(QLatin1Char('?'))) {
                dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
                dir.setNameFilters({part});
                for (const QString &name : dir.entryList())
                    next.append(dir.filePath(name));
            } else {
                const QString candidate = dir.filePath(part);
                const QFileInfo info(candidate);
                if (info.isDir() && !info.isSymLink())
                    next.append(candidate);
            }
        }
        current = next;
    }
    return current;
}

} // namespace

UpdateController::UpdateController(UpdateService *service, QString currentVersion, QString currentBuildId,
                                   bool isDesktopPlatform, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_currentVersion(std::move(currentVersion))
    , m_currentBuildId(std::move(currentBuildId))
    , m_supported(!isDesktopPlatform || qEnvironmentVariable("NEXORA_UPDATE_TEST") == QLatin1String("1"))
{
    connect(m_service, &UpdateService::stateChanged, this, &UpdateController::stateChanged);
    connect(m_service, &UpdateService::progressChanged, this, &UpdateController::progressChanged);
}

QString UpdateController::state() const
{
    return update::nameOf(m_service->state()).toLower();
}

QString UpdateController::error() const
{
    return m_service->error() == update::UpdateError::None ? QString() : update::nameOf(m_service->error());
}

QString UpdateController::message() const
{
    if (!m_supported && m_service->state() == UpdateState::Idle)
        return QStringLiteral("Updates are installed only on the Nexora OS image");
    return m_service->message();
}

double UpdateController::progress() const
{
    return m_service->progress();
}

QString UpdateController::availableVersion() const
{
    const auto pending = m_service->pending();
    return pending ? pending->version.toString() : QString();
}

QString UpdateController::notes() const
{
    const auto pending = m_service->pending();
    return pending ? pending->notes : QString();
}

bool UpdateController::checkBundle(const QString &dir)
{
    return m_supported && m_service->checkBundle(dir);
}

bool UpdateController::verify()
{
    return m_supported && m_service->verify();
}

bool UpdateController::install()
{
    return m_supported && m_service->install();
}

void UpdateController::cancel()
{
    m_service->cancel();
}

QStringList UpdateController::candidateBundles() const
{
    QStringList all;
    all += expandDirectoryGlob(QStringLiteral("/media/*/nexora-update-*"));
    all += expandDirectoryGlob(QStringLiteral("/run/media/*/*/nexora-update-*"));
    all += expandDirectoryGlob(QDir::homePath() + QStringLiteral("/nexora-update-*"));
    all.removeDuplicates();
    return all;
}

} // namespace nexora::ui
