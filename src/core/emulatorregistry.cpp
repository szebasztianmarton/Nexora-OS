#include "emulatorregistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <algorithm>

namespace nexora {

EmulatorRegistry::EmulatorRegistry(const QString &configDirectory, QObject *parent)
    : QAbstractListModel(parent)
    , m_configPath(QDir(configDirectory).filePath(QStringLiteral("emulators.json")))
{
    reload();
}

int EmulatorRegistry::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_definitions.size());
}

QVariant EmulatorRegistry::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const EmulatorDefinition &def = m_definitions.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return def.name;
    case IdRole: return def.id;
    case ExecutableRole: return def.executable;
    case ArgumentsRole: return def.arguments;
    case WorkingDirectoryRole: return def.workingDirectory;
    case EnabledRole: return def.enabled;
    case PlatformsRole: return def.platforms;
    case ExtensionsRole: return def.extensions;
    case ProfilesRole: return def.toVariantMap().value(QStringLiteral("profiles"));
    case StatusRole: return EmulatorDefinition::statusKey(def.status);
    case StatusTextRole: return EmulatorDefinition::statusText(def.status);
    case ValidationMessagesRole: return def.validationMessages;
    case CanLaunchRole: return def.canLaunch();
    case GameCountRole: return m_gameCounts.value(def.id, 0);
    default: return {};
    }
}

QHash<int, QByteArray> EmulatorRegistry::roleNames() const
{
    return {{IdRole, "id"},
            {NameRole, "name"},
            {ExecutableRole, "executable"},
            {ArgumentsRole, "arguments"},
            {WorkingDirectoryRole, "workingDirectory"},
            {EnabledRole, "enabled"},
            {PlatformsRole, "platforms"},
            {ExtensionsRole, "extensions"},
            {ProfilesRole, "profiles"},
            {StatusRole, "status"},
            {StatusTextRole, "statusText"},
            {ValidationMessagesRole, "validationMessages"},
            {CanLaunchRole, "canLaunch"},
            {GameCountRole, "gameCount"}};
}

int EmulatorRegistry::count() const { return int(m_definitions.size()); }

int EmulatorRegistry::availableCount() const
{
    return int(std::count_if(m_definitions.cbegin(), m_definitions.cend(),
                             [](const EmulatorDefinition &d) { return d.canLaunch(); }));
}

QString EmulatorRegistry::configPath() const { return m_configPath; }
bool EmulatorRegistry::configExists() const { return QFile::exists(m_configPath); }
QString EmulatorRegistry::loadError() const { return m_loadError; }
QStringList EmulatorRegistry::skippedEntries() const { return m_skipped; }

QStringList EmulatorRegistry::supportedExtensions() const
{
    QStringList result;
    for (const EmulatorDefinition &def : m_definitions) {
        if (!def.enabled)
            continue;
        for (const QString &ext : def.extensions) {
            if (!result.contains(ext))
                result.append(ext);
        }
    }
    result.sort();
    return result;
}

void EmulatorRegistry::reload()
{
    const int oldCount = count();
    beginResetModel();
    m_definitions.clear();
    m_skipped.clear();
    m_loadError.clear();

    // emulators.json is edited by hand, so it is read directly (never renamed
    // or rewritten, unlike JsonStore files).
    QFile file(m_configPath);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            m_loadError = QStringLiteral("Cannot read emulators.json: %1").arg(file.errorString());
        } else {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            const QJsonObject root = doc.object();
            const QJsonValue version = root.value(QStringLiteral("version"));
            if (parseError.error != QJsonParseError::NoError) {
                m_loadError = QStringLiteral("emulators.json is not valid JSON: %1 (at offset %2)")
                                  .arg(parseError.errorString())
                                  .arg(parseError.offset);
            } else if (!doc.isObject()) {
                m_loadError = QStringLiteral("emulators.json must contain a JSON object");
            } else if (!version.isUndefined() && version.toInt(-1) != 1) {
                m_loadError = QStringLiteral("emulators.json has an unsupported version (expected 1)");
            } else if (!root.value(QStringLiteral("emulators")).isArray()) {
                m_loadError = QStringLiteral("emulators.json has no \"emulators\" list");
            } else {
                QSet<QString> ids;
                int index = 0;
                for (const QJsonValue &value : root.value(QStringLiteral("emulators")).toArray()) {
                    ++index;
                    if (!value.isObject()) {
                        m_skipped.append(QStringLiteral("entry #%1: not an object").arg(index));
                        continue;
                    }
                    bool ok = false;
                    EmulatorDefinition def = EmulatorDefinition::fromJson(value.toObject(), &ok);
                    if (!ok) {
                        m_skipped.append(QStringLiteral("entry #%1: missing id").arg(index));
                        continue;
                    }
                    if (ids.contains(def.id)) {
                        m_skipped.append(QStringLiteral("entry #%1: duplicate id \"%2\"").arg(index).arg(def.id));
                        continue;
                    }
                    ids.insert(def.id);
                    m_definitions.append(def);
                }
            }
        }
    }
    endResetModel();

    if (!m_loadError.isEmpty())
        qWarning("Emulators: %s", qPrintable(m_loadError));
    for (const QString &skipped : std::as_const(m_skipped))
        qWarning("Emulators: skipped %s", qPrintable(skipped));
    qInfo("Emulators: %d configured, %d available", count(), availableCount());

    if (oldCount != count())
        emit countChanged();
    emit statusChanged();
    emit reloaded();
}

void EmulatorRegistry::validateAll()
{
    bool changed = false;
    for (int row = 0; row < m_definitions.size(); ++row) {
        EmulatorDefinition &def = m_definitions[row];
        const EmulatorStatus before = def.status;
        const QStringList messagesBefore = def.validationMessages;
        def.validate();
        if (def.status != before || def.validationMessages != messagesBefore) {
            changed = true;
            emit dataChanged(index(row), index(row));
        }
    }
    if (changed)
        emit statusChanged();
}

void EmulatorRegistry::validate(const QString &id)
{
    const int row = indexOf(id);
    if (row < 0)
        return;
    EmulatorDefinition &def = m_definitions[row];
    const EmulatorStatus before = def.status;
    const QStringList messagesBefore = def.validationMessages;
    def.validate();
    if (def.status != before || def.validationMessages != messagesBefore) {
        emit dataChanged(index(row), index(row));
        emit statusChanged();
    }
}

QVariantMap EmulatorRegistry::get(int row) const
{
    if (row < 0 || row >= m_definitions.size())
        return {};
    QVariantMap map = m_definitions.at(row).toVariantMap();
    map.insert(QStringLiteral("gameCount"), m_gameCounts.value(m_definitions.at(row).id, 0));
    return map;
}

QVariantMap EmulatorRegistry::byId(const QString &id) const
{
    return get(indexOf(id));
}

int EmulatorRegistry::indexOf(const QString &id) const
{
    for (int row = 0; row < m_definitions.size(); ++row) {
        if (m_definitions.at(row).id == id)
            return row;
    }
    return -1;
}

QString EmulatorRegistry::emulatorForExtension(const QString &extension) const
{
    QString fallback;
    for (const EmulatorDefinition &def : m_definitions) {
        if (!def.enabled || !def.handlesExtension(extension))
            continue;
        if (def.canLaunch())
            return def.id;
        if (fallback.isEmpty())
            fallback = def.id;
    }
    return fallback;
}

const EmulatorDefinition *EmulatorRegistry::find(const QString &id) const
{
    const int row = indexOf(id);
    return row < 0 ? nullptr : &m_definitions.at(row);
}

void EmulatorRegistry::setGameCounts(const QHash<QString, int> &countsByEmulatorId)
{
    if (m_gameCounts == countsByEmulatorId)
        return;
    m_gameCounts = countsByEmulatorId;
    if (!m_definitions.isEmpty())
        emit dataChanged(index(0), index(int(m_definitions.size()) - 1), {GameCountRole});
}

} // namespace nexora
