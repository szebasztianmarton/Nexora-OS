#include "diagnostics.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

#include "controllermanager.h"
#include "emulatormanager.h"
#include "emulatorregistry.h"
#include "gamelibrary.h"
#include "logger.h"

namespace nexora {

namespace {

constexpr int ReportErrorCount = 20;

QHash<QString, QString> readKeyValueFile(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        QString value = line.mid(eq + 1).trimmed();
        if (value.size() >= 2 && (value.startsWith(QLatin1Char('"')) || value.startsWith(QLatin1Char('\'')))
            && value.endsWith(value.at(0)))
            value = value.mid(1, value.size() - 2);
        values.insert(line.left(eq).trimmed(), value);
    }
    return values;
}

QString versionFrom(const QHash<QString, QString> &values)
{
    QString version = values.value(QStringLiteral("VERSION"));
    if (version.isEmpty())
        version = values.value(QStringLiteral("VERSION_ID"));
    const QString build = values.value(QStringLiteral("BUILD_ID"));
    if (version.isEmpty())
        return build;
    if (!build.isEmpty() && build != version)
        return QStringLiteral("%1 (%2)").arg(version, build);
    return version;
}

QString resultText(EmulatorManager::Result result)
{
    switch (result) {
    case EmulatorManager::NoResult: return QStringLiteral("none");
    case EmulatorManager::NormalExit: return QStringLiteral("normal exit");
    case EmulatorManager::ErrorExit: return QStringLiteral("error exit");
    case EmulatorManager::Crashed: return QStringLiteral("crashed");
    case EmulatorManager::FailedToStart: return QStringLiteral("failed to start");
    }
    return {};
}

QString stateText(EmulatorManager::State state)
{
    switch (state) {
    case EmulatorManager::Idle: return QStringLiteral("idle");
    case EmulatorManager::Launching: return QStringLiteral("launching");
    case EmulatorManager::Running: return QStringLiteral("running");
    case EmulatorManager::Stopping: return QStringLiteral("stopping");
    }
    return {};
}

} // namespace

Diagnostics::Diagnostics(const QString &configDirectory, EmulatorRegistry *registry, EmulatorManager *manager,
                         ControllerManager *controllers, GameLibrary *library, QObject *parent)
    : QObject(parent)
    , m_configDirectory(configDirectory)
    , m_registry(registry)
    , m_manager(manager)
    , m_controllers(controllers)
    , m_library(library)
{
}

QString Diagnostics::appVersion() const { return QStringLiteral(NEXORA_VERSION); }
QString Diagnostics::buildId() const { return QStringLiteral(NEXORA_BUILD_ID); }
QString Diagnostics::osName() const { return QSysInfo::prettyProductName(); }
QString Diagnostics::kernel() const { return QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion(); }
QString Diagnostics::architecture() const { return QSysInfo::currentCpuArchitecture(); }
QString Diagnostics::qtVersion() const { return QString::fromLatin1(qVersion()); }
QString Diagnostics::logFilePath() const { return Logger::logFilePath(); }
QString Diagnostics::configDirectory() const { return m_configDirectory; }
QStringList Diagnostics::recentErrors() const { return Logger::recentErrors(); }

QString Diagnostics::imageVersion() const
{
    const QHash<QString, QString> release = readKeyValueFile(QStringLiteral("/etc/nexora-release"));
    if (!release.isEmpty())
        return versionFrom(release);
    const QHash<QString, QString> os = readKeyValueFile(QStringLiteral("/etc/os-release"));
    if (os.value(QStringLiteral("ID")) == QLatin1String("nexora"))
        return versionFrom(os);
    return {};
}

void Diagnostics::refresh()
{
    if (m_registry)
        m_registry->validateAll();
    emit refreshed();
}

QStringList Diagnostics::logTail(int maxLines) const
{
    QStringList lines = Logger::tail(maxLines);
    for (QString &line : lines)
        line = sanitize(line);
    return lines;
}

QString Diagnostics::report() const
{
    QString text;
    QTextStream out(&text);
    const auto orNone = [](const QString &s) { return s.isEmpty() ? QStringLiteral("(none)") : s; };

    out << "Nexora OS diagnostics report\n"
        << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    out << "[Application]\n"
        << "Version: " << appVersion() << "\n"
        << "Build: " << buildId() << "\n"
        << "Image version: " << orNone(imageVersion()) << "\n"
        << "Qt: " << qtVersion() << "\n\n";

    out << "[System]\n"
        << "OS: " << osName() << "\n"
        << "Kernel: " << kernel() << "\n"
        << "Architecture: " << architecture() << "\n\n";

    out << "[Paths]\n"
        << "Config directory: " << m_configDirectory << "\n"
        << "Log file: " << orNone(logFilePath()) << "\n\n";

    out << "[Emulators]\n";
    if (m_registry) {
        out << "Config file: " << m_registry->configPath()
            << (m_registry->configExists() ? "" : " (not found)") << "\n";
        if (!m_registry->loadError().isEmpty())
            out << "Load error: " << m_registry->loadError() << "\n";
        out << "Configured: " << m_registry->count() << ", available: " << m_registry->availableCount() << "\n";
        for (const EmulatorDefinition &def : m_registry->definitions())
            out << "- " << def.name << " [" << def.id << "]: " << EmulatorDefinition::statusText(def.status) << "\n";
        for (const QString &skipped : m_registry->skippedEntries())
            out << "- skipped " << skipped << "\n";
    } else {
        out << "Unavailable\n";
    }
    out << "\n";

    out << "[Launcher]\n";
    if (m_manager) {
        out << "State: " << stateText(m_manager->state()) << "\n"
            << "Last result: " << resultText(m_manager->lastResult());
        if (m_manager->lastResult() != EmulatorManager::NoResult)
            out << " (exit code " << m_manager->lastExitCode() << ")";
        out << "\n";
        if (!m_manager->lastErrorMessage().isEmpty())
            out << "Last error: " << m_manager->lastErrorMessage() << "\n";
    } else {
        out << "Unavailable\n";
    }
    out << "\n";

    out << "[Controllers]\n";
    if (m_controllers) {
        out << "Available: " << (m_controllers->available() ? "yes" : "no") << "\n";
        if (!m_controllers->available())
            out << "Reason: " << m_controllers->unavailableReason() << "\n";
        out << "Connected: " << m_controllers->count() << "\n";
        for (const QVariant &c : m_controllers->controllers()) {
            const QVariantMap map = c.toMap();
            out << "- " << map.value(QStringLiteral("name")).toString() << " ("
                << map.value(QStringLiteral("type")).toString() << ")\n";
        }
    } else {
        out << "Unavailable\n";
    }
    out << "\n";

    out << "[Library]\n";
    if (m_library) {
        out << "Games: " << m_library->model()->count() << "\n"
            << "Scanning: " << (m_library->scanning() ? "yes" : "no") << "\n"
            << "Skipped locations: " << m_library->skippedEntries().size() << "\n";
        for (const QString &skipped : m_library->skippedEntries())
            out << "- " << skipped << "\n";
        if (!m_library->stateError().isEmpty())
            out << "State error: " << m_library->stateError() << "\n";
    } else {
        out << "Unavailable\n";
    }
    out << "\n";

    out << "[Recent errors]\n";
    QStringList errors = Logger::recentErrors();
    if (errors.size() > ReportErrorCount)
        errors = errors.mid(errors.size() - ReportErrorCount);
    if (errors.isEmpty())
        out << "(none)\n";
    for (const QString &error : std::as_const(errors))
        out << error << "\n";

    out.flush();
    return sanitize(text);
}

bool Diagnostics::copyToClipboard(const QString &text) const
{
    if (!qGuiApp)
        return false;
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return false;
    clipboard->setText(text);
    return true;
}

void Diagnostics::clearRecentErrors()
{
    Logger::clearRecentErrors();
    emit refreshed();
}

QString Diagnostics::sanitize(const QString &text)
{
    const QString home = QDir::homePath();
    if (home.isEmpty() || home == QLatin1String("/"))
        return text;
    // Only whole path components: "/home/ann" must not rewrite "/home/anna".
    const QRegularExpression pattern(QRegularExpression::escape(home) + QStringLiteral("(?=/|$|[^\\w.-])"),
                                     QRegularExpression::MultilineOption);
    QString result = text;
    result.replace(pattern, QStringLiteral("~"));
    return result;
}

} // namespace nexora
