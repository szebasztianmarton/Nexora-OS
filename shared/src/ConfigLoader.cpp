#include "nexora/shared/ConfigLoader.h"

#include "nexora/shared/Logging.h"
#include "nexora/shared/SystemCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include <cmath>

namespace nexora::shared {

namespace {

const QStringList &knownKeys()
{
    static const QStringList keys{
        QStringLiteral("id"),          QStringLiteral("name"),          QStringLiteral("executable"),
        QStringLiteral("arguments"),   QStringLiteral("enabled"),       QStringLiteral("workingDirectory"),
        QStringLiteral("extensions"),  QStringLiteral("stopTimeoutMs"), QStringLiteral("profiles"),
        QStringLiteral("version"),     QStringLiteral("systems"),       QStringLiteral("pausable"),
        QStringLiteral("capabilities")};
    return keys;
}

constexpr int MaxCapabilityArguments = 32;

QString jsonTypeName(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null: return QStringLiteral("null");
    case QJsonValue::Bool: return QStringLiteral("boolean");
    case QJsonValue::Double: return QStringLiteral("number");
    case QJsonValue::String: return QStringLiteral("string");
    case QJsonValue::Array: return QStringLiteral("array");
    case QJsonValue::Object: return QStringLiteral("object");
    case QJsonValue::Undefined: break;
    }
    return QStringLiteral("missing");
}

bool isInteger(const QJsonValue &v, double min, double max)
{
    const double d = v.toDouble(-1);
    return v.isDouble() && d == std::floor(d) && d >= min && d <= max;
}

// Parses "capabilities.saveState". Problems are returned in |problems|; any problem drops
// the whole capability (the emulator stays usable without save states).
std::optional<SaveStateCapability> parseSaveState(const QJsonValue &value, QStringList *problems, QStringList *warnings)
{
    const qsizetype problemsBefore = problems->size();
    auto problem = [&](const QString &m) { problems->append(QStringLiteral("\"capabilities.saveState\": ") + m); };
    if (!value.isObject()) {
        problem(QStringLiteral("must be an object (got %1)").arg(jsonTypeName(value)));
        return std::nullopt;
    }
    const QJsonObject o = value.toObject();
    static const QStringList keys{QStringLiteral("adapter"), QStringLiteral("timeoutMs"),
                                  QStringLiteral("launchArguments"), QStringLiteral("resumeArguments"),
                                  QStringLiteral("port")};
    for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
        if (!keys.contains(it.key()) && warnings)
            warnings->append(QStringLiteral("unknown key \"%1\" in \"capabilities.saveState\" ignored").arg(it.key()));
    }

    SaveStateCapability cap;
    const QString adapter = o.value(QStringLiteral("adapter")).toString();
    if (adapter == QLatin1String("control-socket"))
        cap.adapter = SaveStateCapability::Adapter::ControlSocket;
    else if (adapter == QLatin1String("retroarch"))
        cap.adapter = SaveStateCapability::Adapter::RetroArch;
    else
        problem(QStringLiteral("\"adapter\" must be \"control-socket\" or \"retroarch\""));

    if (o.contains(QStringLiteral("timeoutMs"))) {
        if (!isInteger(o.value(QStringLiteral("timeoutMs")), SaveStateCapability::MinTimeoutMs,
                       SaveStateCapability::MaxTimeoutMs))
            problem(QStringLiteral("\"timeoutMs\" must be an integer between %1 and %2")
                        .arg(SaveStateCapability::MinTimeoutMs)
                        .arg(SaveStateCapability::MaxTimeoutMs));
        else
            cap.timeoutMs = o.value(QStringLiteral("timeoutMs")).toInt();
    }

    auto arguments = [&](const QString &key, QStringList *out) {
        if (!o.contains(key))
            return;
        const QJsonValue v = o.value(key);
        if (!v.isArray() || v.toArray().size() > MaxCapabilityArguments) {
            problem(
                QStringLiteral("\"%1\" must be an array of at most %2 strings").arg(key).arg(MaxCapabilityArguments));
            return;
        }
        for (const QJsonValue &a : v.toArray()) {
            if (!a.isString() || a.toString().isEmpty()) {
                problem(QStringLiteral("\"%1\" entries must be non-empty strings").arg(key));
                return;
            }
            out->append(a.toString());
        }
    };
    arguments(QStringLiteral("launchArguments"), &cap.launchArguments);
    arguments(QStringLiteral("resumeArguments"), &cap.resumeArguments);
    auto mentions = [](const QStringList &list, const QString &placeholder) {
        return std::any_of(list.cbegin(), list.cend(), [&](const QString &a) { return a.contains(placeholder); });
    };

    if (cap.adapter == SaveStateCapability::Adapter::ControlSocket) {
        if (!mentions(cap.launchArguments, QStringLiteral("{control-socket}")))
            problem(QStringLiteral("\"launchArguments\" must pass \"{control-socket}\" to the emulator"));
        if (!mentions(cap.resumeArguments, QStringLiteral("{state-file}")))
            problem(QStringLiteral("\"resumeArguments\" must pass \"{state-file}\" to the emulator"));
        if (o.contains(QStringLiteral("port")) && warnings)
            warnings->append(QStringLiteral("\"capabilities.saveState.port\" is only used by the retroarch adapter"));
    } else {
        if ((o.contains(QStringLiteral("launchArguments")) || o.contains(QStringLiteral("resumeArguments")))
            && warnings)
            warnings->append(QStringLiteral("\"capabilities.saveState\": the retroarch adapter builds its own "
                                            "arguments; \"launchArguments\" and \"resumeArguments\" are ignored"));
        cap.launchArguments.clear();
        cap.resumeArguments.clear();
        if (o.contains(QStringLiteral("port"))) {
            if (!isInteger(o.value(QStringLiteral("port")), 1024, 65535))
                problem(QStringLiteral("\"port\" must be an integer between 1024 and 65535"));
            else
                cap.port = o.value(QStringLiteral("port")).toInt();
        }
    }
    if (problems->size() > problemsBefore)
        return std::nullopt;
    return cap;
}

} // namespace

QString ConfigIssue::toDisplayString() const
{
    const QString fileName = QFileInfo(file).fileName();
    return fileName.isEmpty() ? message : fileName + QStringLiteral(": ") + message;
}

bool ConfigLoadResult::hasErrors() const
{
    return std::any_of(issues.cbegin(), issues.cend(),
                       [](const ConfigIssue &issue) { return issue.severity == ConfigIssue::Severity::Error; });
}

bool ConfigLoader::isValidId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    return pattern.match(id).hasMatch();
}

std::optional<EmulatorConfig> ConfigLoader::parse(const QByteArray &json, const QString &sourceFile,
                                                  QStringList *errors, QStringList *warnings,
                                                  const SystemCatalog *catalog)
{
    QStringList localErrors;
    auto fail = [&](const QString &message) { localErrors.append(message); };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        fail(QStringLiteral("invalid JSON at offset %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    } else if (!doc.isObject()) {
        fail(QStringLiteral("top-level JSON value must be an object"));
    }
    if (!localErrors.isEmpty()) {
        if (errors)
            errors->append(localErrors);
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();
    EmulatorConfig config;
    config.sourceFile = sourceFile;

    const QJsonValue id = obj.value(QStringLiteral("id"));
    if (!id.isString())
        fail(QStringLiteral("\"id\" must be a string (got %1)").arg(jsonTypeName(id)));
    else if (!isValidId(id.toString()))
        fail(QStringLiteral("\"id\" \"%1\" is invalid: use 1-64 lowercase letters, digits, '-' or '_', "
                            "starting with a letter or digit")
                 .arg(id.toString()));
    else
        config.id = id.toString();

    const QJsonValue name = obj.value(QStringLiteral("name"));
    if (!name.isString())
        fail(QStringLiteral("\"name\" must be a string (got %1)").arg(jsonTypeName(name)));
    else if (name.toString().trimmed().isEmpty())
        fail(QStringLiteral("\"name\" must not be empty"));
    else
        config.name = name.toString().trimmed();

    const QString baseDir = sourceFile.isEmpty() ? QDir::currentPath() : QFileInfo(sourceFile).absolutePath();
    // Relative paths are resolved against the config file's directory; never via PATH.
    auto resolvePath = [&](const QString &raw) {
        return QDir::cleanPath(QDir::isRelativePath(raw) ? QDir(baseDir).filePath(raw) : raw);
    };
    auto parseStringArray = [&](const QJsonValue &value, const QString &label, QStringList *out) {
        if (!value.isArray()) {
            fail(QStringLiteral("%1 must be an array of strings (got %2)").arg(label, jsonTypeName(value)));
            return;
        }
        const QJsonArray array = value.toArray();
        for (qsizetype i = 0; i < array.size(); ++i) {
            if (!array.at(i).isString()) {
                fail(QStringLiteral("%1[%2] must be a string (got %3)")
                         .arg(label)
                         .arg(i)
                         .arg(jsonTypeName(array.at(i))));
                continue;
            }
            out->append(array.at(i).toString());
        }
    };
    auto parseOptionalPath = [&](const QJsonObject &o, const QString &label, QString *out) {
        if (!o.contains(QStringLiteral("workingDirectory")))
            return;
        const QJsonValue v = o.value(QStringLiteral("workingDirectory"));
        if (!v.isString() || v.toString().trimmed().isEmpty())
            fail(QStringLiteral("%1 must be a non-empty string").arg(label));
        else
            *out = resolvePath(v.toString().trimmed());
    };
    auto parseOptionalBool = [&](const QJsonObject &o, const QString &label, bool *out) {
        if (!o.contains(QStringLiteral("enabled")))
            return;
        const QJsonValue v = o.value(QStringLiteral("enabled"));
        if (!v.isBool())
            fail(QStringLiteral("%1 must be a boolean (got %2)").arg(label, jsonTypeName(v)));
        else
            *out = v.toBool();
    };

    const QJsonValue executable = obj.value(QStringLiteral("executable"));
    if (!executable.isString()) {
        fail(QStringLiteral("\"executable\" must be a string (got %1)").arg(jsonTypeName(executable)));
    } else if (executable.toString().trimmed().isEmpty()) {
        fail(QStringLiteral("\"executable\" must not be empty"));
    } else {
        config.executable = resolvePath(executable.toString().trimmed());
    }

    if (obj.contains(QStringLiteral("arguments")))
        parseStringArray(obj.value(QStringLiteral("arguments")), QStringLiteral("\"arguments\""), &config.arguments);
    parseOptionalBool(obj, QStringLiteral("\"enabled\""), &config.enabled);
    parseOptionalPath(obj, QStringLiteral("\"workingDirectory\""), &config.workingDirectory);

    if (obj.contains(QStringLiteral("extensions"))) {
        static const QRegularExpression extPattern(QStringLiteral("^\\.[a-z0-9]{1,16}$"));
        QStringList raw;
        parseStringArray(obj.value(QStringLiteral("extensions")), QStringLiteral("\"extensions\""), &raw);
        for (const QString &e : std::as_const(raw)) {
            const QString ext = e.trimmed().toLower();
            if (!extPattern.match(ext).hasMatch())
                fail(QStringLiteral("\"extensions\" entry \"%1\" must look like \".ext\"").arg(e));
            else if (!config.extensions.contains(ext))
                config.extensions.append(ext);
        }
    }

    if (obj.contains(QStringLiteral("stopTimeoutMs"))) {
        const QJsonValue v = obj.value(QStringLiteral("stopTimeoutMs"));
        const double ms = v.toDouble(-1);
        if (!v.isDouble() || ms != std::floor(ms) || ms < MinStopTimeoutMs || ms > MaxStopTimeoutMs)
            fail(QStringLiteral("\"stopTimeoutMs\" must be an integer between %1 and %2")
                     .arg(MinStopTimeoutMs)
                     .arg(MaxStopTimeoutMs));
        else
            config.stopTimeoutMs = int(ms);
    }

    if (obj.contains(QStringLiteral("version"))) {
        const QJsonValue v = obj.value(QStringLiteral("version"));
        if (!v.isString() || v.toString().size() > MaxVersionLength)
            fail(QStringLiteral("\"version\" must be a string of at most %1 characters").arg(MaxVersionLength));
        else
            config.version = v.toString().trimmed();
    }

    if (obj.contains(QStringLiteral("systems"))) {
        QStringList raw;
        parseStringArray(obj.value(QStringLiteral("systems")), QStringLiteral("\"systems\""), &raw);
        for (const QString &systemId : std::as_const(raw)) {
            if (!SystemCatalog::isValidId(systemId)) {
                if (warnings)
                    warnings->append(
                        QStringLiteral("\"systems\" entry \"%1\" is not a valid system id; ignored").arg(systemId));
            } else if (catalog && !catalog->find(systemId)) {
                if (warnings)
                    warnings->append(
                        QStringLiteral("\"systems\" entry \"%1\" is not a known system; ignored").arg(systemId));
            } else if (!config.systems.contains(systemId)) {
                config.systems.append(systemId);
            }
        }
    }

    if (obj.contains(QStringLiteral("pausable"))) {
        const QJsonValue v = obj.value(QStringLiteral("pausable"));
        if (!v.isBool())
            fail(QStringLiteral("\"pausable\" must be a boolean (got %1)").arg(jsonTypeName(v)));
        else
            config.pausable = v.toBool();
    }

    if (obj.contains(QStringLiteral("capabilities"))) {
        // A malformed block only drops the capability: the emulator stays usable, and the
        // problem is reported as a warning (docs/emulator-integration.md#capabilities).
        const QJsonValue v = obj.value(QStringLiteral("capabilities"));
        QStringList problems;
        if (!v.isObject()) {
            problems.append(QStringLiteral("\"capabilities\" must be an object (got %1)").arg(jsonTypeName(v)));
        } else {
            const QJsonObject caps = v.toObject();
            for (auto it = caps.constBegin(); it != caps.constEnd(); ++it) {
                if (it.key() != QLatin1String("pause") && it.key() != QLatin1String("saveState") && warnings)
                    warnings->append(QStringLiteral("unknown key \"%1\" in \"capabilities\" ignored").arg(it.key()));
            }
            if (caps.contains(QStringLiteral("pause"))) {
                const QJsonValue pause = caps.value(QStringLiteral("pause"));
                const QJsonValue pausable = obj.value(QStringLiteral("pausable"));
                if (!pause.isBool()) {
                    problems.append(
                        QStringLiteral("\"capabilities.pause\" must be a boolean (got %1)").arg(jsonTypeName(pause)));
                } else if (pausable.isBool() && pausable.toBool() != pause.toBool()) {
                    // Pausing a game that cannot take SIGSTOP is worse than not offering it.
                    problems.append(QStringLiteral("\"pausable\" and \"capabilities.pause\" disagree; "
                                                   "the emulator is treated as not pausable"));
                    config.pausable = false;
                } else {
                    config.pausable = pause.toBool();
                }
            }
            if (caps.contains(QStringLiteral("saveState")))
                config.saveState = parseSaveState(caps.value(QStringLiteral("saveState")), &problems, warnings);
        }
        if (warnings) {
            for (const QString &p : std::as_const(problems))
                warnings->append(p + QStringLiteral("; capability ignored"));
        }
    }

    if (obj.contains(QStringLiteral("profiles"))) {
        const QJsonValue profiles = obj.value(QStringLiteral("profiles"));
        if (!profiles.isArray()) {
            fail(QStringLiteral("\"profiles\" must be an array of objects (got %1)").arg(jsonTypeName(profiles)));
        } else {
            static const QStringList profileKeys{QStringLiteral("id"),
                                                 QStringLiteral("name"),
                                                 QStringLiteral("emulator"),
                                                 QStringLiteral("arguments"),
                                                 QStringLiteral("workingDirectory"),
                                                 QStringLiteral("enabled")};
            const QJsonArray array = profiles.toArray();
            if (array.size() > MaxProfiles)
                fail(QStringLiteral("\"profiles\" must not contain more than %1 entries").arg(MaxProfiles));
            for (qsizetype i = 0; i < array.size() && i < MaxProfiles; ++i) {
                const QString label = QStringLiteral("\"profiles\"[%1]").arg(i);
                if (!array.at(i).isObject()) {
                    fail(QStringLiteral("%1 must be an object").arg(label));
                    continue;
                }
                const QJsonObject p = array.at(i).toObject();
                LaunchProfile profile;
                profile.emulatorId = config.id;
                const QJsonValue pid = p.value(QStringLiteral("id"));
                if (!pid.isString() || !isValidId(pid.toString()))
                    fail(QStringLiteral("%1.id must be a valid id (lowercase letters, digits, '-', '_')").arg(label));
                else if (config.profile(pid.toString()))
                    fail(QStringLiteral("%1.id \"%2\" is used by another profile").arg(label, pid.toString()));
                else
                    profile.id = pid.toString();
                const QJsonValue pname = p.value(QStringLiteral("name"));
                if (!pname.isString() || pname.toString().trimmed().isEmpty())
                    fail(QStringLiteral("%1.name must be a non-empty string").arg(label));
                else
                    profile.name = pname.toString().trimmed();
                if (p.contains(QStringLiteral("emulator"))
                    && p.value(QStringLiteral("emulator")).toString() != config.id) {
                    fail(QStringLiteral("%1.emulator must match the emulator id \"%2\"").arg(label, config.id));
                }
                if (p.contains(QStringLiteral("arguments"))) {
                    profile.hasArguments = true;
                    parseStringArray(p.value(QStringLiteral("arguments")), label + QStringLiteral(".arguments"),
                                     &profile.arguments);
                }
                parseOptionalPath(p, label + QStringLiteral(".workingDirectory"), &profile.workingDirectory);
                parseOptionalBool(p, label + QStringLiteral(".enabled"), &profile.enabled);
                for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
                    if (!profileKeys.contains(it.key()) && warnings)
                        warnings->append(QStringLiteral("unknown key \"%1\" in %2 ignored").arg(it.key(), label));
                }
                if (!profile.id.isEmpty())
                    config.profiles.append(profile);
            }
        }
    }

    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        if (!knownKeys().contains(it.key()) && warnings)
            warnings->append(QStringLiteral("unknown key \"%1\" ignored").arg(it.key()));
    }

    if (!localErrors.isEmpty()) {
        if (errors)
            errors->append(localErrors);
        return std::nullopt;
    }
    return config;
}

ConfigLoadResult ConfigLoader::loadDirectory(const QString &directory, const SystemCatalog *catalog)
{
    ConfigLoadResult result;
    const QDir dir(directory);
    if (directory.isEmpty() || !dir.exists()) {
        result.issues.append({ConfigIssue::Severity::Error, directory,
                              QStringLiteral("configuration directory \"%1\" does not exist").arg(directory)});
        return result;
    }
    result.directoryFound = true;

    // Only regular *.json files directly inside the directory; hidden files skipped.
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QHash<QString, QString> seenIds; // id -> file that defined it

    for (const QFileInfo &info : files) {
        const QString path = info.absoluteFilePath();
        auto addIssue = [&](ConfigIssue::Severity severity, const QString &message) {
            result.issues.append({severity, path, message});
            if (severity == ConfigIssue::Severity::Error)
                qCWarning(lcConfig).noquote() << info.fileName() << ":" << message;
            else
                qCInfo(lcConfig).noquote() << info.fileName() << ":" << message;
        };

        if (info.size() > MaxFileSize) {
            addIssue(ConfigIssue::Severity::Error, QStringLiteral("file is larger than %1 bytes").arg(MaxFileSize));
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            addIssue(ConfigIssue::Severity::Error, QStringLiteral("cannot read file: %1").arg(file.errorString()));
            continue;
        }

        QStringList errors;
        QStringList warnings;
        const auto config = parse(file.readAll(), path, &errors, &warnings, catalog);
        for (const QString &warning : std::as_const(warnings))
            addIssue(ConfigIssue::Severity::Warning, warning);
        for (const QString &error : std::as_const(errors))
            addIssue(ConfigIssue::Severity::Error, error);
        if (!config)
            continue;

        if (const auto it = seenIds.constFind(config->id); it != seenIds.constEnd()) {
            addIssue(ConfigIssue::Severity::Error,
                     QStringLiteral("duplicate id \"%1\" (already defined in %2); entry ignored")
                         .arg(config->id, QFileInfo(it.value()).fileName()));
            continue;
        }
        seenIds.insert(config->id, path);
        result.emulators.append(*config);
    }

    qCInfo(lcConfig) << "Loaded" << result.emulators.size() << "emulator definition(s) from" << directory << "with"
                     << result.issues.size() << "issue(s)";
    return result;
}

} // namespace nexora::shared
