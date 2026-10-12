#include "nexora/shared/Settings.h"

#include "nexora/shared/Logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>

namespace nexora::shared {

namespace {

using Validator = std::function<QString(QVariant &)>;

Validator oneOf(const QStringList &allowed)
{
    return [allowed](QVariant &v) -> QString {
        if (v.typeId() != QMetaType::QString)
            return QStringLiteral("must be a string");
        const QString s = v.toString().trimmed();
        if (!allowed.contains(s))
            return QStringLiteral("must be one of: %1").arg(allowed.join(QStringLiteral(", ")));
        v = s;
        return {};
    };
}

Validator boolean()
{
    return [](QVariant &v) -> QString {
        return v.typeId() == QMetaType::Bool ? QString() : QStringLiteral("must be true or false");
    };
}

Validator intRange(int min, int max)
{
    return [min, max](QVariant &v) -> QString {
        bool ok = false;
        const double d = v.toDouble(&ok);
        const bool numeric
            = v.typeId() == QMetaType::Int || v.typeId() == QMetaType::Double || v.typeId() == QMetaType::LongLong;
        if (!numeric || !ok || d != std::floor(d) || d < min || d > max)
            return QStringLiteral("must be an integer between %1 and %2").arg(min).arg(max);
        v = int(d);
        return {};
    };
}

QString toStringList(QVariant &v, const std::function<QString(QString &)> &item, int maxItems)
{
    if (v.typeId() != QMetaType::QVariantList && v.typeId() != QMetaType::QStringList)
        return QStringLiteral("must be an array of strings");
    QStringList out;
    for (const QVariant &e : v.toList()) {
        if (e.typeId() != QMetaType::QString)
            return QStringLiteral("must be an array of strings");
        QString s = e.toString().trimmed();
        if (const QString err = item(s); !err.isEmpty())
            return err;
        if (!out.contains(s))
            out.append(s);
    }
    if (out.size() > maxItems)
        return QStringLiteral("must not contain more than %1 entries").arg(maxItems);
    v = out;
    return {};
}

Validator directories()
{
    return [](QVariant &v) {
        return toStringList(
            v,
            [](QString &s) -> QString {
                if (s.isEmpty() || QDir::isRelativePath(s))
                    return QStringLiteral("entries must be absolute directory paths");
                if (QDir::cleanPath(s) == QStringLiteral("/"))
                    return QStringLiteral("scanning the whole filesystem (\"/\") is not allowed");
                s = QDir::cleanPath(s);
                return {};
            },
            64);
    };
}

Validator extensions()
{
    return [](QVariant &v) {
        static const QRegularExpression re(QStringLiteral("^\\.[a-z0-9]{1,16}$"));
        return toStringList(
            v,
            [](QString &s) -> QString {
                s = s.toLower();
                if (!s.startsWith(u'.'))
                    s.prepend(u'.');
                if (!re.match(s).hasMatch())
                    return QStringLiteral("entries must look like \".ext\" (1-16 letters/digits)");
                return {};
            },
            128);
    };
}

Validator resolution()
{
    return [](QVariant &v) -> QString {
        static const QRegularExpression re(QStringLiteral("^(auto|[1-9][0-9]{2,4}x[1-9][0-9]{2,4})$"));
        if (v.typeId() != QMetaType::QString || !re.match(v.toString().trimmed()).hasMatch())
            return QStringLiteral("must be \"auto\" or WIDTHxHEIGHT, e.g. \"1920x1080\"");
        v = v.toString().trimmed();
        return {};
    };
}

Validator stringMap(const QRegularExpression &keyRe, const QRegularExpression &valueRe, const QString &what)
{
    return [keyRe, valueRe, what](QVariant &v) -> QString {
        if (v.typeId() != QMetaType::QVariantMap)
            return QStringLiteral("must be an object");
        QVariantMap out;
        const QVariantMap in = v.toMap();
        for (auto it = in.cbegin(); it != in.cend(); ++it) {
            if (it.value().typeId() != QMetaType::QString || !keyRe.match(it.key()).hasMatch()
                || !valueRe.match(it.value().toString()).hasMatch()) {
                return QStringLiteral("invalid entry \"%1\": %2").arg(it.key(), what);
            }
            out.insert(it.key(), it.value().toString());
        }
        v = out;
        return {};
    };
}

QVariantMap defaultMapping()
{
    return {{QStringLiteral("accept"), QStringLiteral("south")},  {QStringLiteral("back"), QStringLiteral("east")},
            {QStringLiteral("menu"), QStringLiteral("start")},    {QStringLiteral("favorite"), QStringLiteral("north")},
            {QStringLiteral("search"), QStringLiteral("west")},   {QStringLiteral("prevTab"), QStringLiteral("l1")},
            {QStringLiteral("nextTab"), QStringLiteral("r1")},    {QStringLiteral("gameBar"), QStringLiteral("guide")},
            {QStringLiteral("options"), QStringLiteral("select")}};
}

} // namespace

const QList<Settings::Spec> &Settings::specs()
{
    static const QRegularExpression id(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    static const QRegularExpression action(
        QStringLiteral("^(accept|back|menu|favorite|search|prevTab|nextTab|gameBar|options)$"));
    static const QRegularExpression button(QStringLiteral("^(south|east|west|north|start|select|guide|l1|r1)$"));
    static const QList<Spec> list{
        {UiLanguage, QStringLiteral("en"), QStringLiteral("Interface language"), oneOf({QStringLiteral("en")})},
        {UiShowHints, true, QStringLiteral("Show the control hint bar"), boolean()},
        {LibrarySortOrder, QStringLiteral("title"), QStringLiteral("Default game library sort order"),
         oneOf(
             {QStringLiteral("title"), QStringLiteral("recent"), QStringLiteral("emulator"), QStringLiteral("added")})},
        {LibraryDirectories, QStringList{}, QStringLiteral("Directories scanned for games"), directories()},
        {LibraryExtensions, QStringList{},
         QStringLiteral("Extra file extensions to include (emulator definitions declare their own)"), extensions()},
        {LibraryFollowSymlinks, false, QStringLiteral("Follow symbolic links that stay inside a library directory"),
         boolean()},
        {DisplayMode, QStringLiteral("fullscreen"), QStringLiteral("Window mode"),
         oneOf({QStringLiteral("fullscreen"), QStringLiteral("windowed")})},
        {DisplayResolution, QStringLiteral("auto"), QStringLiteral("Preferred resolution"), resolution()},
        {AudioVolume, 80, QStringLiteral("Master volume preference (0-100)"), intRange(0, 100)},
        {InputBackend, QStringLiteral("auto"), QStringLiteral("Controller backend"),
         oneOf({QStringLiteral("auto"), QStringLiteral("evdev"), QStringLiteral("sdl3"), QStringLiteral("none")})},
        {InputMapping, defaultMapping(), QStringLiteral("Controller button for each action"),
         stringMap(action, button, QStringLiteral("expected action -> button name"))},
        {EmulatorDefaultProfiles, QVariantMap{}, QStringLiteral("Default launch profile per emulator id"),
         stringMap(id, id, QStringLiteral("expected emulator id -> profile id"))},
    };
    return list;
}

const Settings::Spec *Settings::spec(const QString &key)
{
    for (const Spec &s : specs()) {
        if (s.key == key)
            return &s;
    }
    return nullptr;
}

QString Settings::defaultUserFile()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/nexora/settings.json");
}

QStringList Settings::defaultSystemFiles(const QString &applicationDir)
{
    return {QDir::cleanPath(applicationDir + QStringLiteral("/../share/nexora/settings-defaults.json")),
            QStringLiteral("/etc/nexora/settings-defaults.json")};
}

Settings::Settings(Paths paths, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
{ }

std::optional<QVariantMap> Settings::readLayer(const QString &file, Layer layer)
{
    const QString name = layer == Layer::User ? QStringLiteral("user settings") : QStringLiteral("system defaults");
    QFile f(file);
    if (!f.exists())
        return QVariantMap{};
    auto problem = [&](const QString &message) {
        m_issues.append(QStringLiteral("%1 (%2): %3").arg(name, file, message));
        qCWarning(lcConfig).noquote() << m_issues.last();
    };
    if (f.size() > MaxFileSize) {
        problem(QStringLiteral("file is larger than %1 bytes").arg(MaxFileSize));
        return std::nullopt;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        problem(QStringLiteral("cannot read: %1").arg(f.errorString()));
        return std::nullopt;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        problem(err.error != QJsonParseError::NoError
                    ? QStringLiteral("invalid JSON at offset %1: %2").arg(err.offset).arg(err.errorString())
                    : QStringLiteral("top-level value must be an object"));
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt(-1);
    if (version != FormatVersion) {
        problem(QStringLiteral("unsupported or missing \"version\" (expected %1)").arg(FormatVersion));
        return std::nullopt;
    }
    if (!root.value(QStringLiteral("settings")).isObject()) {
        problem(QStringLiteral("\"settings\" must be an object"));
        return std::nullopt;
    }

    QVariantMap result;
    const QVariantMap raw = root.value(QStringLiteral("settings")).toObject().toVariantMap();
    for (auto it = raw.cbegin(); it != raw.cend(); ++it) {
        const Spec *s = spec(it.key());
        if (!s) {
            problem(QStringLiteral("unknown setting \"%1\" ignored").arg(it.key()));
            continue;
        }
        QVariant v = it.value();
        if (const QString e = s->validate(v); !e.isEmpty()) {
            problem(QStringLiteral("\"%1\" %2; using the default").arg(it.key(), e));
            continue;
        }
        result.insert(it.key(), v);
    }
    return result;
}

bool Settings::load(LoadMode mode)
{
    m_issues.clear();
    m_system.clear();
    m_user.clear();
    for (const QString &file : std::as_const(m_paths.systemDefaultFiles)) {
        if (const auto layer = readLayer(file, Layer::System))
            m_system.insert(*layer);
    }

    bool ok = true;
    if (!m_paths.userFile.isEmpty()) {
        if (const auto layer = readLayer(m_paths.userFile, Layer::User)) {
            m_user = *layer;
        } else {
            ok = false;
            if (mode == LoadMode::Inspect) {
                emit reloaded();
                return ok;
            }
            // Keep the broken file for inspection and start from defaults.
            const QString aside = m_paths.userFile + QStringLiteral(".corrupt-")
                + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
            if (QFile::rename(m_paths.userFile, aside))
                m_issues.append(QStringLiteral("user settings were unusable and moved to %1").arg(aside));
        }
    }
    qCInfo(lcConfig) << "Settings loaded:" << m_user.size() << "user override(s)," << m_system.size()
                     << "system default(s)," << m_issues.size() << "issue(s)";
    emit reloaded();
    return ok;
}

bool Settings::save()
{
    if (m_paths.userFile.isEmpty())
        return false;
    QDir().mkpath(QFileInfo(m_paths.userFile).absolutePath());
    QJsonObject root{{QStringLiteral("version"), FormatVersion},
                     {QStringLiteral("settings"), QJsonObject::fromVariantMap(m_user)}};
    QSaveFile file(m_paths.userFile);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(lcConfig) << "Cannot write settings" << m_paths.userFile << file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qCWarning(lcConfig) << "Cannot write settings" << m_paths.userFile << file.errorString();
        return false;
    }
    return true;
}

bool Settings::resetToDefaults()
{
    const QVariantMap previous = m_user;
    m_user.clear();
    if (!m_paths.userFile.isEmpty() && QFile::exists(m_paths.userFile)) {
        const QString backup = m_paths.userFile + QStringLiteral(".bak");
        QFile::remove(backup);
        if (!QFile::rename(m_paths.userFile, backup)) {
            qCWarning(lcConfig) << "Cannot move" << m_paths.userFile << "aside";
            return false;
        }
    }
    for (auto it = previous.cbegin(); it != previous.cend(); ++it)
        emit valueChanged(it.key(), value(it.key()));
    qCInfo(lcConfig) << "Settings reset to defaults";
    return true;
}

QVariant Settings::layeredDefault(const QString &key) const
{
    if (m_system.contains(key))
        return m_system.value(key);
    const Spec *s = spec(key);
    return s ? s->defaultValue : QVariant();
}

QVariant Settings::value(const QString &key) const
{
    const auto it = m_user.constFind(key);
    return it != m_user.cend() ? *it : layeredDefault(key);
}

bool Settings::setValue(const QString &key, const QVariant &value, QString *error)
{
    const Spec *s = spec(key);
    if (!s) {
        if (error)
            *error = QStringLiteral("unknown setting \"%1\"").arg(key);
        return false;
    }
    QVariant v = value;
    if (const QString e = s->validate(v); !e.isEmpty()) {
        if (error)
            *error = QStringLiteral("\"%1\" %2").arg(key, e);
        return false;
    }
    const QVariant before = this->value(key);
    if (v == layeredDefault(key))
        m_user.remove(key);
    else
        m_user.insert(key, v);
    if (before != v)
        emit valueChanged(key, v);
    return true;
}

} // namespace nexora::shared
