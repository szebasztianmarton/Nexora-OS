#include "SystemController.h"

#include "nexora/system/SystemIntegration.h"

#include <QGuiApplication>
#include <QScreen>

namespace nexora::ui {

SystemController::SystemController(system::SystemIntegration *system, QObject *parent)
    : QObject(parent)
    , m_system(system)
{
    // Keep the display lines current when monitors are connected or removed.
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(app, &QGuiApplication::screenAdded, this, &SystemController::refresh);
        connect(app, &QGuiApplication::screenRemoved, this, &SystemController::refresh);
    }
    refresh();
}

QString SystemController::platform() const
{
    return m_system->platformName();
}

bool SystemController::powerSupported() const
{
    return m_system->supports(system::Capability::Power);
}

void SystemController::refresh()
{
    const system::SystemStatus s = m_system->status();
    QStringList lines;
    lines.append(QStringLiteral("Platform: %1").arg(s.platform));
    if (!s.osName.isEmpty())
        lines.append(QStringLiteral("OS: %1 (kernel %2)").arg(s.osName, s.kernel));
    lines.append(QStringLiteral("Host: %1").arg(s.hostname));
    if (s.uptimeSeconds >= 0)
        lines.append(
            QStringLiteral("Uptime: %1 h %2 min").arg(s.uptimeSeconds / 3600).arg((s.uptimeSeconds / 60) % 60));
    if (s.load1 >= 0)
        lines.append(QStringLiteral("Load (1 min): %1").arg(s.load1, 0, 'f', 2));
    if (s.memoryTotalKiB > 0)
        lines.append(QStringLiteral("Memory: %1 MiB available of %2 MiB")
                         .arg(s.memoryAvailableKiB / 1024)
                         .arg(s.memoryTotalKiB / 1024));
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        for (QScreen *screen : app->screens())
            lines.append(QStringLiteral("Display %1: %2x%3 @ %4 Hz")
                             .arg(screen->name())
                             .arg(screen->size().width())
                             .arg(screen->size().height())
                             .arg(screen->refreshRate(), 0, 'f', 0));
    }
    const system::UpdateInfo update = m_system->updateInfo();
    lines.append(QStringLiteral("Updates: %1%2")
                     .arg(update.installedVersion.isEmpty() ? QString()
                                                            : QStringLiteral("image %1, ").arg(update.installedVersion),
                          update.message));
    m_lines = lines;
    emit statusChanged();
}

bool SystemController::powerOff()
{
    QString error;
    const bool ok = m_system->requestPower(system::PowerAction::Poweroff, &error);
    m_lastError = error;
    emit statusChanged();
    return ok;
}

bool SystemController::reboot()
{
    QString error;
    const bool ok = m_system->requestPower(system::PowerAction::Reboot, &error);
    m_lastError = error;
    emit statusChanged();
    return ok;
}

} // namespace nexora::ui
