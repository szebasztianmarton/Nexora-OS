#include "nexora/emulator/SaveStateAdapter.h"

#include "nexora/shared/AtomicFile.h"
#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUdpSocket>
#include <QUuid>

namespace nexora::emulator {

using shared::SaveStateCapability;

namespace {

QStringList substitute(QStringList arguments, const QString &placeholder, const QString &value)
{
    for (QString &a : arguments)
        a.replace(placeholder, value);
    return arguments;
}

QString seconds(int ms)
{
    return QString::number((ms + 999) / 1000);
}

} // namespace

// --- SaveStateAdapter ----------------------------------------------------------------------

SaveStateAdapter::SaveStateAdapter(const SaveStateCapability &capability, QObject *parent)
    : QObject(parent)
    , m_capability(capability)
{
    m_timeout.setSingleShot(true);
}

std::unique_ptr<SaveStateAdapter> SaveStateAdapter::create(const SaveStateCapability &capability, QObject *parent)
{
    switch (capability.adapter) {
    case SaveStateCapability::Adapter::ControlSocket: return std::make_unique<ControlSocketAdapter>(capability, parent);
    case SaveStateCapability::Adapter::RetroArch: return std::make_unique<RetroArchAdapter>(capability, parent);
    }
    return nullptr;
}

QString SaveStateAdapter::adapterName(SaveStateCapability::Adapter adapter)
{
    switch (adapter) {
    case SaveStateCapability::Adapter::ControlSocket: return QStringLiteral("control-socket");
    case SaveStateCapability::Adapter::RetroArch: return QStringLiteral("retroarch");
    }
    return {};
}

bool SaveStateAdapter::begin()
{
    if (m_busy)
        return false;
    m_busy = true;
    m_timeout.start(m_capability.timeoutMs);
    return true;
}

void SaveStateAdapter::endSave(bool ok, const QString &file, const QString &error)
{
    m_timeout.stop();
    m_busy = false;
    if (ok)
        qCInfo(lcProcess).noquote() << "Save state written:" << file;
    else
        qCWarning(lcProcess).noquote() << "Save state failed:" << error;
    emit saveFinished(ok, ok ? file : QString(), ok ? QString() : error);
}

void SaveStateAdapter::endQuit(bool ok, const QString &error)
{
    m_timeout.stop();
    m_busy = false;
    emit quitFinished(ok, ok ? QString() : error);
}

bool SaveStateAdapter::checkStateFile(const QString &file, const QString &directory, QString *error)
{
    const QFileInfo info(file);
    const QString dir = QFileInfo(directory).canonicalFilePath();
    const QString canonical = info.canonicalFilePath();
    if (!info.isAbsolute() || canonical.isEmpty() || dir.isEmpty() || !canonical.startsWith(dir + u'/')) {
        *error = QStringLiteral("The emulator reported a state file outside the session directory");
        return false;
    }
    // Only the session directory itself: a name from a subdirectory would be recorded (and
    // later resolved) as if it were directly inside it.
    if (QFileInfo(canonical).path() != dir) {
        *error = QStringLiteral("The emulator reported a state file that is not directly in the session directory");
        return false;
    }
    if (info.fileName() == QLatin1String("meta.json")) {
        *error = QStringLiteral("The emulator reported the session's meta.json as its state file");
        return false;
    }
    if (info.isSymLink() || !info.isFile()) {
        *error = QStringLiteral("The emulator's state file is not a regular file");
        return false;
    }
    if (info.size() <= 0) {
        *error = QStringLiteral("The emulator wrote an empty state file");
        return false;
    }
    return true;
}

// --- ControlSocketAdapter ------------------------------------------------------------------

ControlSocketAdapter::ControlSocketAdapter(const SaveStateCapability &capability, QObject *parent)
    : SaveStateAdapter(capability, parent)
{
    connect(&m_timeout, &QTimer::timeout, this, &ControlSocketAdapter::onTimeout);
}

ControlSocketAdapter::~ControlSocketAdapter()
{
    // No signals from the destructor: the session is going away.
    m_pending = Pending::None;
    if (m_client)
        m_client->disconnect(this);
    if (m_server)
        m_server->close();
}

std::optional<QStringList> ControlSocketAdapter::prepareLaunch(const SaveStateLaunch &launch, QString *error)
{
    close();
    if (!launch.resumeFile.isEmpty() && !QFileInfo(launch.resumeFile).isFile()) {
        *error = QStringLiteral("The saved state %1 does not exist").arg(launch.resumeFile);
        return std::nullopt;
    }
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    m_server->setMaxPendingConnections(1);
    // Short name in the temporary directory: Unix socket paths are limited to ~100 bytes,
    // the session directory may be longer.
    const QString name = QStringLiteral("nexora-ss-") + QUuid::createUuid().toString(QUuid::Id128).left(16);
    if (!m_server->listen(name)) {
        *error = QStringLiteral("Could not open the emulator control socket: %1").arg(m_server->errorString());
        delete m_server;
        m_server = nullptr;
        return std::nullopt;
    }
    connect(m_server, &QLocalServer::newConnection, this, &ControlSocketAdapter::onNewConnection);

    QStringList arguments
        = substitute(capability().launchArguments, QStringLiteral("{control-socket}"), m_server->fullServerName());
    if (!launch.resumeFile.isEmpty())
        arguments.append(substitute(capability().resumeArguments, QStringLiteral("{state-file}"),
                                    QFileInfo(launch.resumeFile).absoluteFilePath()));
    return arguments;
}

QString ControlSocketAdapter::socketPath() const
{
    return m_server ? m_server->fullServerName() : QString();
}

bool ControlSocketAdapter::isReady() const
{
    return m_client && m_client->state() == QLocalSocket::ConnectedState;
}

void ControlSocketAdapter::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        if (m_client) {
            qCWarning(lcProcess) << "Refused a second connection to the emulator control socket";
            socket->disconnectFromServer();
            socket->deleteLater();
            continue;
        }
        m_client = socket;
        m_buffer.clear();
        connect(socket, &QLocalSocket::readyRead, this, &ControlSocketAdapter::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &ControlSocketAdapter::onDisconnected);
        qCInfo(lcProcess) << "Emulator connected to the control socket";
        emit readyChanged(true);
    }
}

bool ControlSocketAdapter::send(const QByteArray &line)
{
    if (!isReady())
        return false;
    return m_client->write(line) == line.size();
}

void ControlSocketAdapter::save(const QString &directory)
{
    if (!begin()) {
        emit saveFinished(false, {}, QStringLiteral("Another save-state request is still running"));
        return;
    }
    if (!isReady()) {
        endSave(false, {}, QStringLiteral("The emulator is not connected to Nexora's control socket"));
        return;
    }
    if (directory.contains(u'\n') || !QFileInfo(directory).isDir()) {
        endSave(false, {}, QStringLiteral("Invalid save-state directory"));
        return;
    }
    m_pending = Pending::Save;
    m_saveDirectory = directory;
    if (!send("SAVE " + QFileInfo(directory).absoluteFilePath().toUtf8() + '\n'))
        fail(QStringLiteral("Could not send the save request to the emulator"));
}

void ControlSocketAdapter::requestQuit()
{
    if (!begin()) {
        emit quitFinished(false, QStringLiteral("Another save-state request is still running"));
        return;
    }
    if (!isReady()) {
        endQuit(false, QStringLiteral("The emulator is not connected to Nexora's control socket"));
        return;
    }
    m_pending = Pending::Quit;
    if (!send("QUIT\n"))
        fail(QStringLiteral("Could not send the quit request to the emulator"));
}

void ControlSocketAdapter::onReadyRead()
{
    m_buffer += m_client->readAll();
    for (qsizetype newline = m_buffer.indexOf('\n'); newline >= 0; newline = m_buffer.indexOf('\n')) {
        const QString line = QString::fromUtf8(m_buffer.left(newline)).trimmed();
        m_buffer.remove(0, newline + 1);
        const Pending pending = m_pending;
        m_pending = Pending::None;
        if (pending == Pending::None) {
            qCWarning(lcProcess).noquote()
                << "Unexpected line on the emulator control socket ignored:" << line.left(200);
            continue;
        }
        const bool ok = line == QLatin1String("OK") || line.startsWith(QLatin1String("OK "));
        const QString rest = line.section(u' ', 1).trimmed();
        if (!ok && !line.startsWith(QLatin1String("ERR"))) {
            m_pending = pending;
            fail(QStringLiteral("Unexpected answer from the emulator: %1").arg(line.left(200)));
            continue;
        }
        if (pending == Pending::Quit) {
            endQuit(ok, ok ? QString() : QStringLiteral("The emulator refused to quit: %1").arg(rest));
            continue;
        }
        if (!ok) {
            endSave(false, {}, QStringLiteral("The emulator could not save: %1").arg(rest.left(500)));
            continue;
        }
        QString error;
        if (rest.isEmpty())
            endSave(false, {}, QStringLiteral("The emulator did not name the state file"));
        else if (checkStateFile(rest, m_saveDirectory, &error))
            endSave(true, QFileInfo(rest).absoluteFilePath(), {});
        else
            endSave(false, {}, error);
    }
    if (m_buffer.size() > MaxLineBytes) {
        m_buffer.clear();
        fail(QStringLiteral("The emulator sent a line longer than %1 bytes").arg(MaxLineBytes));
    }
}

void ControlSocketAdapter::onDisconnected()
{
    if (m_client)
        m_client->deleteLater();
    m_client = nullptr;
    emit readyChanged(false);
    if (m_pending != Pending::None)
        fail(QStringLiteral("The emulator closed the control connection"));
}

void ControlSocketAdapter::onTimeout()
{
    fail(QStringLiteral("The emulator did not answer within %1 s").arg(seconds(capability().timeoutMs)));
}

void ControlSocketAdapter::fail(const QString &error)
{
    const Pending pending = m_pending;
    m_pending = Pending::None;
    if (pending == Pending::Save)
        endSave(false, {}, error);
    else if (pending == Pending::Quit)
        endQuit(false, error);
}

void ControlSocketAdapter::close()
{
    if (m_pending != Pending::None)
        fail(QStringLiteral("The emulator session ended"));
    if (m_client) {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = nullptr;
    }
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

// --- RetroArchAdapter ----------------------------------------------------------------------

RetroArchAdapter::RetroArchAdapter(const SaveStateCapability &capability, QObject *parent)
    : SaveStateAdapter(capability, parent)
{
    m_poll.setInterval(100);
    connect(&m_poll, &QTimer::timeout, this, &RetroArchAdapter::poll);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_poll.stop();
        endSave(
            false, {},
            QStringLiteral("RetroArch did not write a state within %1 s").arg(seconds(this->capability().timeoutMs)));
    });
}

RetroArchAdapter::~RetroArchAdapter() = default;

std::optional<QStringList> RetroArchAdapter::prepareLaunch(const SaveStateLaunch &launch, QString *error)
{
    close();
    const QString sessionDir = QFileInfo(launch.sessionDir).absoluteFilePath();
    if (sessionDir.contains(u'"') || sessionDir.contains(u'\n')) {
        *error = QStringLiteral("The session directory cannot be written to a RetroArch configuration");
        return std::nullopt;
    }
    m_stateDir = sessionDir + QStringLiteral("/retroarch");
    if (!QDir().mkpath(m_stateDir)) {
        *error = QStringLiteral("Could not create %1").arg(m_stateDir);
        return std::nullopt;
    }
    QStringList arguments;
    if (!launch.resumeFile.isEmpty()) {
        const QString base = QFileInfo(launch.gamePath).completeBaseName();
        if (base.isEmpty() || !QFileInfo(launch.resumeFile).isFile()) {
            *error = QStringLiteral("The saved state %1 does not exist").arg(launch.resumeFile);
            return std::nullopt;
        }
        // RetroArch loads slot 0 from <savestate_directory>/<content base name>.state. That
        // file is the adapter's working copy; the session's own states are never touched.
        const QString target = m_stateDir + u'/' + base + QStringLiteral(".state");
        QFile::remove(target);
        if (!QFile::copy(launch.resumeFile, target)) {
            *error = QStringLiteral("Could not prepare the saved state for RetroArch");
            return std::nullopt;
        }
    }
    const QByteArray config = QStringLiteral("# Written by Nexora for one game session; do not edit.\n"
                                             "network_cmd_enable = \"true\"\n"
                                             "network_cmd_port = \"%1\"\n"
                                             "savestate_directory = \"%2\"\n"
                                             "sort_savestates_enable = \"false\"\n"
                                             "sort_savestates_by_content_enable = \"false\"\n"
                                             "savestate_auto_index = \"false\"\n"
                                             "savestate_auto_save = \"false\"\n"
                                             "savestate_auto_load = \"false\"\n"
                                             "savestate_thumbnail_enable = \"false\"\n"
                                             "state_slot = \"0\"\n"
                                             "quit_press_twice = \"false\"\n"
                                             "confirm_quit = \"false\"\n"
                                             "config_save_on_exit = \"false\"\n")
                                  .arg(capability().port)
                                  .arg(m_stateDir)
                                  .toUtf8();
    const QString configPath = sessionDir + u'/' + configFileName();
    QString writeError;
    if (!shared::writeFileAtomically(configPath, config, &writeError)) {
        *error = QStringLiteral("Could not write the RetroArch configuration: %1").arg(writeError);
        return std::nullopt;
    }
    arguments << QStringLiteral("--appendconfig") << configPath;
    if (!launch.resumeFile.isEmpty())
        arguments << QStringLiteral("--entryslot") << QStringLiteral("0");
    if (!m_socket)
        m_socket = new QUdpSocket(this);
    m_prepared = true;
    return arguments;
}

RetroArchAdapter::Snapshot RetroArchAdapter::newestState(const QString &directory)
{
    const QFileInfoList files
        = QDir(directory).entryInfoList({QStringLiteral("*.state")}, QDir::Files | QDir::NoSymLinks, QDir::Time);
    if (files.isEmpty())
        return {};
    return {files.first().absoluteFilePath(), files.first().size(), files.first().lastModified()};
}

bool RetroArchAdapter::sendCommand(const QByteArray &command, QString *error)
{
    if (!m_prepared || !m_socket) {
        *error = QStringLiteral("RetroArch was not started with Nexora's configuration");
        return false;
    }
    if (m_socket->writeDatagram(command + '\n', QHostAddress::LocalHost, quint16(capability().port))
        != command.size() + 1) {
        *error = QStringLiteral("Could not send %1 to RetroArch: %2")
                     .arg(QString::fromLatin1(command), m_socket->errorString());
        return false;
    }
    return true;
}

void RetroArchAdapter::save(const QString &directory)
{
    if (!begin()) {
        emit saveFinished(false, {}, QStringLiteral("Another save-state request is still running"));
        return;
    }
    if (!QFileInfo(directory).isDir()) {
        endSave(false, {}, QStringLiteral("Invalid save-state directory"));
        return;
    }
    m_saveDirectory = directory;
    m_before = newestState(m_stateDir);
    m_last = {};
    QString error;
    if (!sendCommand("SAVE_STATE", &error)) {
        endSave(false, {}, error);
        return;
    }
    m_poll.start();
}

void RetroArchAdapter::poll()
{
    const Snapshot now = newestState(m_stateDir);
    const bool changed = !now.file.isEmpty()
        && (now.file != m_before.file || now.size != m_before.size || now.modified != m_before.modified);
    if (!changed)
        return;
    // Done once the file has the same size and time on two polls in a row.
    if (now.size <= 0 || now.file != m_last.file || now.size != m_last.size || now.modified != m_last.modified) {
        m_last = now;
        return;
    }
    m_poll.stop();
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString target = m_saveDirectory + QStringLiteral("/state-") + stamp + QStringLiteral(".state");
    for (int n = 1; QFileInfo::exists(target); ++n)
        target = m_saveDirectory + QStringLiteral("/state-%1-%2.state").arg(stamp).arg(n);
    if (!QFile::copy(now.file, target)) {
        endSave(false, {}, QStringLiteral("Could not copy RetroArch's state file"));
        return;
    }
    QString error;
    if (checkStateFile(target, m_saveDirectory, &error))
        endSave(true, target, {});
    else
        endSave(false, {}, error);
}

void RetroArchAdapter::requestQuit()
{
    if (!begin()) {
        emit quitFinished(false, QStringLiteral("Another save-state request is still running"));
        return;
    }
    QString error;
    const bool ok = sendCommand("QUIT", &error);
    endQuit(ok, error);
}

void RetroArchAdapter::close()
{
    m_poll.stop();
    if (isBusy())
        endSave(false, {}, QStringLiteral("The emulator session ended"));
    m_prepared = false;
}

} // namespace nexora::emulator
