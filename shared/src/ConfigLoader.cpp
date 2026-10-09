#include "nexora/shared/ConfigLoader.h"

#include "nexora/shared/Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace nexora::shared {

namespace {

const QStringList &knownKeys()
{
    static const QStringList keys{QStringLiteral("id"), QStringLiteral("name"),
                                  QStringLiteral("executable"), QStringLiteral("arguments"),
                                  QStringLiteral("enabled")};
    return keys;
}

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

} // namespace

QString ConfigIssue::toDisplayString() const
{
    const QString fileName = QFileInfo(file).fileName();
    return fileName.isEmpty() ? message : fileName + QStringLiteral(": ") + message;
}

bool ConfigLoadResult::hasErrors() const
{
    return std::any_of(issues.cbegin(), issues.cend(), [](const ConfigIssue &issue) {
        return issue.severity == ConfigIssue::Severity::Error;
    });
}

bool ConfigLoader::isValidId(const QString &id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,63}$"));
    return pattern.match(id).hasMatch();
}

std::optional<EmulatorConfig> ConfigLoader::parse(const QByteArray &json,
                                                  const QString &sourceFile,
                                                  QStringList *errors,
                                                  QStringList *warnings)
{
    QStringList localErrors;
    auto fail = [&](const QString &message) { localErrors.append(message); };

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        fail(QStringLiteral("invalid JSON at offset %1: %2")
                 .arg(parseError.offset)
                 .arg(parseError.errorString()));
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

    const QJsonValue executable = obj.value(QStringLiteral("executable"));
    if (!executable.isString()) {
        fail(QStringLiteral("\"executable\" must be a string (got %1)").arg(jsonTypeName(executable)));
    } else if (executable.toString().trimmed().isEmpty()) {
        fail(QStringLiteral("\"executable\" must not be empty"));
    } else {
        const QString raw = executable.toString().trimmed();
        QString resolved = raw;
        if (QDir::isRelativePath(raw)) {
            const QString baseDir = sourceFile.isEmpty() ? QDir::currentPath()
                                                         : QFileInfo(sourceFile).absolutePath();
            resolved = QDir(baseDir).filePath(raw);
        }
        config.executable = QDir::cleanPath(resolved);
    }

    if (obj.contains(QStringLiteral("arguments"))) {
        const QJsonValue arguments = obj.value(QStringLiteral("arguments"));
        if (!arguments.isArray()) {
            fail(QStringLiteral("\"arguments\" must be an array of strings (got %1)")
                     .arg(jsonTypeName(arguments)));
        } else {
            const QJsonArray array = arguments.toArray();
            for (qsizetype i = 0; i < array.size(); ++i) {
                if (!array.at(i).isString()) {
                    fail(QStringLiteral("\"arguments\"[%1] must be a string (got %2)")
                             .arg(i)
                             .arg(jsonTypeName(array.at(i))));
                    continue;
                }
                config.arguments.append(array.at(i).toString());
            }
        }
    }

    if (obj.contains(QStringLiteral("enabled"))) {
        const QJsonValue enabled = obj.value(QStringLiteral("enabled"));
        if (!enabled.isBool())
            fail(QStringLiteral("\"enabled\" must be a boolean (got %1)").arg(jsonTypeName(enabled)));
        else
            config.enabled = enabled.toBool();
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

ConfigLoadResult ConfigLoader::loadDirectory(const QString &directory)
{
    ConfigLoadResult result;
    const QDir dir(directory);
    if (directory.isEmpty() || !dir.exists()) {
        result.issues.append({ConfigIssue::Severity::Error, directory,
                              QStringLiteral("configuration directory \"%1\" does not exist")
                                  .arg(directory)});
        return result;
    }
    result.directoryFound = true;

    // Only regular *.json files directly inside the directory; hidden files skipped.
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")},
                                                  QDir::Files, QDir::Name);
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
            addIssue(ConfigIssue::Severity::Error,
                     QStringLiteral("file is larger than %1 bytes").arg(MaxFileSize));
            continue;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            addIssue(ConfigIssue::Severity::Error,
                     QStringLiteral("cannot read file: %1").arg(file.errorString()));
            continue;
        }

        QStringList errors;
        QStringList warnings;
        const auto config = parse(file.readAll(), path, &errors, &warnings);
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

    qCInfo(lcConfig) << "Loaded" << result.emulators.size() << "emulator definition(s) from"
                     << directory << "with" << result.issues.size() << "issue(s)";
    return result;
}

} // namespace nexora::shared
