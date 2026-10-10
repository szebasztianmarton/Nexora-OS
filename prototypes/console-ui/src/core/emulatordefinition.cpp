#include "emulatordefinition.h"

#include <QFileInfo>
#include <algorithm>
#include <QJsonArray>
#include <QVariantList>

namespace nexora {

namespace {

// Reads an optional array of strings. Returns false (and leaves `out` empty)
// if the value is present but not an array of strings.
bool readStringList(const QJsonObject &object, const QString &key, QStringList &out)
{
    out.clear();
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull())
        return true;
    if (!value.isArray())
        return false;
    for (const QJsonValue &v : value.toArray()) {
        if (!v.isString()) {
            out.clear();
            return false;
        }
        out.append(v.toString());
    }
    return true;
}

bool readString(const QJsonObject &object, const QString &key, QString &out)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull())
        return true;
    if (!value.isString())
        return false;
    out = value.toString().trimmed();
    return true;
}

QString normalizedExtension(const QString &extension)
{
    QString ext = extension.trimmed().toLower();
    while (ext.startsWith(QLatin1Char('.')))
        ext.remove(0, 1);
    return ext;
}

const QString GamePlaceholder = QStringLiteral("{game}");

} // namespace

EmulatorDefinition EmulatorDefinition::fromJson(const QJsonObject &object, bool *ok)
{
    EmulatorDefinition def;
    QStringList &problems = def.configurationMessages;

    if (!readString(object, QStringLiteral("id"), def.id) || def.id.isEmpty()) {
        if (ok)
            *ok = false;
        problems.append(QStringLiteral("Missing or invalid \"id\""));
        def.status = EmulatorStatus::InvalidConfiguration;
        def.validationMessages = problems;
        return def;
    }
    if (ok)
        *ok = true;

    if (!readString(object, QStringLiteral("name"), def.name) || def.name.isEmpty()) {
        problems.append(QStringLiteral("Missing or invalid \"name\""));
        def.name = def.id;
    }
    if (!readString(object, QStringLiteral("executable"), def.executable) || def.executable.isEmpty())
        problems.append(QStringLiteral("Missing or invalid \"executable\""));
    if (!readStringList(object, QStringLiteral("arguments"), def.arguments))
        problems.append(QStringLiteral("\"arguments\" must be a list of strings"));
    if (!readString(object, QStringLiteral("workingDirectory"), def.workingDirectory))
        problems.append(QStringLiteral("\"workingDirectory\" must be a string"));

    const QJsonValue enabled = object.value(QStringLiteral("enabled"));
    if (enabled.isBool())
        def.enabled = enabled.toBool();
    else if (!enabled.isUndefined() && !enabled.isNull())
        problems.append(QStringLiteral("\"enabled\" must be true or false"));

    if (!readStringList(object, QStringLiteral("platforms"), def.platforms))
        problems.append(QStringLiteral("\"platforms\" must be a list of strings"));
    def.platforms.removeAll(QString());

    QStringList extensions;
    if (!readStringList(object, QStringLiteral("extensions"), extensions))
        problems.append(QStringLiteral("\"extensions\" must be a list of strings"));
    for (const QString &ext : std::as_const(extensions)) {
        const QString normalized = normalizedExtension(ext);
        if (!normalized.isEmpty() && !def.extensions.contains(normalized))
            def.extensions.append(normalized);
    }

    const QJsonValue profiles = object.value(QStringLiteral("profiles"));
    if (profiles.isArray()) {
        int index = 0;
        for (const QJsonValue &v : profiles.toArray()) {
            ++index;
            const QJsonObject p = v.toObject();
            LaunchProfile profile;
            if (!v.isObject() || !readString(p, QStringLiteral("id"), profile.id) || profile.id.isEmpty()) {
                problems.append(QStringLiteral("Profile #%1 has no valid \"id\"").arg(index));
                continue;
            }
            if (!readString(p, QStringLiteral("name"), profile.name) || profile.name.isEmpty())
                profile.name = profile.id;
            if (!readStringList(p, QStringLiteral("arguments"), profile.arguments)) {
                problems.append(QStringLiteral("Profile \"%1\": \"arguments\" must be a list of strings").arg(profile.id));
                continue;
            }
            def.profiles.append(profile);
        }
    } else if (!profiles.isUndefined() && !profiles.isNull()) {
        problems.append(QStringLiteral("\"profiles\" must be a list"));
    }

    def.validate();
    return def;
}

void EmulatorDefinition::validate()
{
    validationMessages = configurationMessages;
    EmulatorStatus problem = configurationMessages.isEmpty() ? EmulatorStatus::Available
                                                             : EmulatorStatus::InvalidConfiguration;
    auto report = [&](EmulatorStatus s, const QString &message) {
        validationMessages.append(message);
        if (problem == EmulatorStatus::Available)
            problem = s;
    };

    if (!executable.isEmpty()) {
        const QFileInfo exe(executable);
        if (exe.isRelative())
            report(EmulatorStatus::InvalidConfiguration,
                   QStringLiteral("Executable path must be absolute: %1").arg(executable));
        else if (!exe.exists())
            report(EmulatorStatus::ExecutableMissing, QStringLiteral("Executable not found: %1").arg(executable));
        else if (!exe.isFile() || !exe.isExecutable())
            report(EmulatorStatus::NotExecutable,
                   QStringLiteral("Not an executable file: %1").arg(executable));
    }
    if (!workingDirectory.isEmpty()) {
        const QFileInfo dir(workingDirectory);
        if (dir.isRelative() || !dir.isDir())
            report(EmulatorStatus::InvalidConfiguration,
                   QStringLiteral("Working directory not found: %1").arg(workingDirectory));
    }
    status = enabled ? problem : EmulatorStatus::Disabled;
}

QStringList EmulatorDefinition::buildArguments(const QString &gamePath, const QString &profileId) const
{
    QStringList args = arguments;
    if (!profileId.isEmpty()) {
        for (const LaunchProfile &profile : profiles) {
            if (profile.id == profileId) {
                args = profile.arguments;
                break;
            }
        }
        if (args == arguments && std::none_of(profiles.cbegin(), profiles.cend(),
                                              [&](const LaunchProfile &p) { return p.id == profileId; }))
            qWarning("Emulators: %s has no profile \"%s\"; using default arguments",
                     qPrintable(id), qPrintable(profileId));
    }
    if (gamePath.isEmpty()) {
        args.removeIf([](const QString &arg) { return arg.contains(GamePlaceholder); });
        return args;
    }
    bool substituted = false;
    for (QString &arg : args) {
        if (arg.contains(GamePlaceholder)) {
            arg.replace(GamePlaceholder, gamePath);
            substituted = true;
        }
    }
    if (!substituted)
        args.append(gamePath);
    return args;
}

bool EmulatorDefinition::handlesExtension(const QString &extension) const
{
    return extensions.contains(normalizedExtension(extension));
}

QString EmulatorDefinition::statusKey(EmulatorStatus status)
{
    switch (status) {
    case EmulatorStatus::Available: return QStringLiteral("available");
    case EmulatorStatus::ExecutableMissing: return QStringLiteral("missing");
    case EmulatorStatus::NotExecutable: return QStringLiteral("not-executable");
    case EmulatorStatus::InvalidConfiguration: return QStringLiteral("invalid");
    case EmulatorStatus::Disabled: return QStringLiteral("disabled");
    case EmulatorStatus::Unknown: break;
    }
    return QStringLiteral("unknown");
}

QString EmulatorDefinition::statusText(EmulatorStatus status)
{
    switch (status) {
    case EmulatorStatus::Available: return QStringLiteral("Available");
    case EmulatorStatus::ExecutableMissing: return QStringLiteral("Executable missing");
    case EmulatorStatus::NotExecutable: return QStringLiteral("Not executable");
    case EmulatorStatus::InvalidConfiguration: return QStringLiteral("Invalid configuration");
    case EmulatorStatus::Disabled: return QStringLiteral("Disabled");
    case EmulatorStatus::Unknown: break;
    }
    return QStringLiteral("Unknown");
}

QVariantMap EmulatorDefinition::toVariantMap() const
{
    QVariantList profileList;
    for (const LaunchProfile &profile : profiles) {
        profileList.append(QVariantMap{{QStringLiteral("id"), profile.id},
                                       {QStringLiteral("name"), profile.name},
                                       {QStringLiteral("arguments"), profile.arguments}});
    }
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("executable"), executable},
            {QStringLiteral("arguments"), arguments},
            {QStringLiteral("workingDirectory"), workingDirectory},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("platforms"), platforms},
            {QStringLiteral("extensions"), extensions},
            {QStringLiteral("profiles"), profileList},
            {QStringLiteral("status"), statusKey(status)},
            {QStringLiteral("statusText"), statusText(status)},
            {QStringLiteral("validationMessages"), validationMessages},
            {QStringLiteral("canLaunch"), canLaunch()}};
}

} // namespace nexora
