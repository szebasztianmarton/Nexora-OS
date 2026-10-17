#include "SystemController.h"

#include "AppController.h"
#include "GameSessionManager.h"
#include "LibraryController.h"
#include "SettingsController.h"

#include "nexora/emulator/LaunchHistory.h"
#include "nexora/library/GameLibrary.h"
#include "nexora/shared/BuildInfo.h"
#include "nexora/shared/Logging.h"
#include "nexora/shared/Redaction.h"
#include "nexora/shared/Settings.h"
#include "nexora/system/BackupService.h"
#include "nexora/system/DiagnosticReport.h"
#include "nexora/system/SystemIntegration.h"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>

#include <utility>

namespace nexora::ui {

namespace {

// One plain-text line per recent launch; see DiagnosticInputs::launchSummaries. Keeps
// nexora_system independent of nexora_emulator_manager: the formatting happens here.
QStringList formatLaunchSummaries(const QList<emulator::LaunchRecord> &records, int limit)
{
    QStringList lines;
    for (const auto &r : records) {
        if (lines.size() >= limit)
            break;
        QString line = r.gameTitle.isEmpty() ? r.emulatorId : r.gameTitle;
        line += QStringLiteral(" - ")
            + (r.endedAt.isValid() ? r.endedAt.toUTC().toString(Qt::ISODate) : QStringLiteral("unknown time"));
        line += QStringLiteral(" - ") + (r.finalState.isEmpty() ? QStringLiteral("unknown") : r.finalState);
        if (r.exitCode)
            line += QStringLiteral(" (exit %1)").arg(*r.exitCode);
        if (r.startupFailure)
            line += QStringLiteral(" [startup failure]");
        lines.append(line);
    }
    return lines;
}

} // namespace

SystemController::SystemController(Dependencies deps, QObject *parent)
    : QObject(parent)
    , m_deps(std::move(deps))
{
    m_buildId = QString::fromLatin1(shared::build::BuildId);
    m_board = QString::fromLatin1(shared::build::Board);
    refreshStatus();
    m_osVersion = m_deps.system->status().osName;
    // Keep the display lines current when monitors are connected or removed.
    if (auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(app, &QGuiApplication::screenAdded, this, &SystemController::refresh);
        connect(app, &QGuiApplication::screenRemoved, this, &SystemController::refresh);
    }
}

QString SystemController::platform() const
{
    return m_deps.system->platformName();
}

bool SystemController::powerSupported() const
{
    return m_deps.system->supports(system::Capability::Power);
}

void SystemController::refreshStatus()
{
    const system::SystemStatus s = m_deps.system->status();
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
    const system::UpdateInfo update = m_deps.system->updateInfo();
    lines.append(QStringLiteral("Updates: %1%2")
                     .arg(update.installedVersion.isEmpty() ? QString()
                                                            : QStringLiteral("image %1, ").arg(update.installedVersion),
                          update.message));
    m_lines = lines;
}

void SystemController::refresh()
{
    refreshStatus();
    emit statusChanged();

    const system::HardwareSnapshot hw
        = system::probeHardware(system::HardwareRoots{}, {m_deps.layout.dataDir, QStringLiteral("/")});
    QVariantMap map;
    map[QStringLiteral("cpuModel")] = hw.cpuModel;
    map[QStringLiteral("cpuThreads")] = hw.cpuThreads;
    map[QStringLiteral("memTotalKiB")] = hw.memTotalKiB;
    map[QStringLiteral("memAvailableKiB")] = hw.memAvailableKiB;
    QVariantList gpus;
    for (const auto &g : hw.gpus) {
        gpus.append(QVariantMap{{QStringLiteral("card"), g.card},
                                {QStringLiteral("vendorId"), g.vendorId},
                                {QStringLiteral("deviceId"), g.deviceId},
                                {QStringLiteral("driver"), g.driver},
                                {QStringLiteral("vendorName"), g.vendorName}});
    }
    map[QStringLiteral("gpus")] = gpus;
    QVariantList outputs;
    for (const auto &o : hw.outputs) {
        outputs.append(QVariantMap{{QStringLiteral("name"), o.name},
                                   {QStringLiteral("connected"), o.connected},
                                   {QStringLiteral("modes"), o.modes}});
    }
    map[QStringLiteral("outputs")] = outputs;
    QVariantList audio;
    for (const auto &a : hw.audio)
        audio.append(QVariantMap{
            {QStringLiteral("index"), a.index}, {QStringLiteral("id"), a.id}, {QStringLiteral("name"), a.name}});
    map[QStringLiteral("audio")] = audio;
    QVariantList storage;
    for (const auto &st : hw.storage) {
        storage.append(QVariantMap{{QStringLiteral("path"), st.path},
                                   {QStringLiteral("device"), st.device},
                                   {QStringLiteral("fsType"), st.fsType},
                                   {QStringLiteral("totalBytes"), st.totalBytes},
                                   {QStringLiteral("availableBytes"), st.availableBytes}});
    }
    map[QStringLiteral("storage")] = storage;
    m_hardware = map;
    ++m_refreshCount;
    m_lastRefresh.start();
    emit hardwareChanged();
}

bool SystemController::refreshIfStale(int maxAgeMs)
{
    if (m_lastRefresh.isValid() && m_lastRefresh.elapsed() < maxAgeMs)
        return false;
    refresh();
    return true;
}

bool SystemController::powerOff()
{
    return requestPower(system::PowerAction::Poweroff);
}

bool SystemController::reboot()
{
    return requestPower(system::PowerAction::Reboot);
}

bool SystemController::requestPower(system::PowerAction action)
{
    auto send = [this, action] {
        QString error;
        const bool ok = m_deps.system->requestPower(action, &error);
        m_lastError = error;
        emit statusChanged();
        return ok;
    };
    // BusyBox init sends SIGTERM to every process, the emulator included, so a running game
    // must be saved (or closed) before the request goes out.
    if (m_sessions && m_deps.app && m_deps.app->isBusy()) {
        m_sessions->closeForPower([send](bool ok) {
            if (ok)
                send();
            else
                qCWarning(lcSystem) << "Power request dropped: the running game could not be saved";
        });
        return true;
    }
    return send();
}

QString SystemController::exportDiagnostics()
{
    system::DiagnosticInputs in;
    in.appVersion = QString::fromLatin1(shared::build::Version);
    in.buildId = m_buildId;
    in.platform = platform();
    in.hardware = system::probeHardware(system::HardwareRoots{}, {m_deps.layout.dataDir, QStringLiteral("/")});
    in.recentLog = shared::recentLogLines();
    in.configIssues = m_deps.app ? m_deps.app->configIssues() : QStringList{};
    in.settingsIssues = m_deps.settings ? m_deps.settings->issues() : QStringList{};
    in.libraryIssues = m_deps.libraryController ? m_deps.libraryController->issues() : QStringList{};
    in.launchSummaries = m_deps.history ? formatLaunchSummaries(m_deps.history->records(), 20) : QStringList{};

    const QString text = system::buildDiagnosticReport(in, shared::defaultRedactionRules());
    QString error;
    const auto path = system::exportDiagnosticReport(text, m_deps.layout.diagnosticsDir, &error);
    m_lastError = path ? QString() : error;
    emit statusChanged();
    return path.value_or(QString());
}

QString SystemController::createBackup(bool includeSaves)
{
    QString error;
    const auto path = m_deps.backup->createBackup(includeSaves, &error);
    m_lastError = path ? QString() : error;
    emit statusChanged();
    return path.value_or(QString());
}

QStringList SystemController::backups() const
{
    return m_deps.backup->listBackups();
}

bool SystemController::restoreBackup(const QString &dir)
{
    if (m_deps.library) {
        m_deps.library->cancelScan();
        m_deps.library->waitForScan();
        m_deps.library->close();
    }
    QString error;
    const bool ok = m_deps.backup->restoreBackup(dir, &error);
    if (m_deps.library)
        m_deps.library->open(m_deps.databasePath);
    if (m_deps.settings)
        m_deps.settings->load();
    // Reloading the store alone leaves live subsystems (volume/mute, controller mapping,
    // deadzone, log level) on their pre-restore values; push the restored settings out too.
    if (m_deps.settingsController)
        m_deps.settingsController->applyAll();
    m_lastError = ok ? QString() : error;
    emit statusChanged();
    return ok;
}

bool SystemController::clearArtworkCache()
{
    QString error;
    for (const QString &dir : m_deps.layout.cacheOnlyDirs()) {
        QDir d(dir);
        if (d.exists() && !d.removeRecursively())
            error = QStringLiteral("cannot clear %1").arg(dir);
        QDir().mkpath(dir);
    }
    m_lastError = error;
    emit statusChanged();
    return error.isEmpty();
}

} // namespace nexora::ui
