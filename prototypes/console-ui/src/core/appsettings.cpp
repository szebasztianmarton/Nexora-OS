#include "appsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

namespace nexora {

namespace {

constexpr qreal MinScale = 0.8;
constexpr qreal MaxScale = 1.3;

const QStringList &validLanguages()
{
    static const QStringList languages{QStringLiteral("en")};
    return languages;
}

const QStringList &validThemes()
{
    static const QStringList themes{QStringLiteral("dark")};
    return themes;
}

QString normalizedPath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

} // namespace

AppSettings::AppSettings(const QString &configDirectory, QObject *parent)
    : QObject(parent)
    , m_store(QDir(configDirectory).filePath(QStringLiteral("settings.json")))
{
    load();
}

QStringList AppSettings::validSortKeys()
{
    return {QStringLiteral("title-asc"), QStringLiteral("title-desc"), QStringLiteral("recent"),
            QStringLiteral("added")};
}

void AppSettings::load()
{
    const QJsonObject obj = m_store.load(&m_loadError);
    if (!m_loadError.isEmpty())
        qWarning("Settings: %s; using defaults", qPrintable(m_loadError));

    auto warnInvalid = [](const char *key) {
        qWarning("Settings: invalid value for \"%s\", using default", key);
    };
    auto readBool = [&](const char *key, bool &target) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isUndefined())
            return;
        if (v.isBool())
            target = v.toBool();
        else
            warnInvalid(key);
    };
    auto readChoice = [&](const char *key, const QStringList &choices, QString &target) {
        const QJsonValue v = obj.value(QLatin1String(key));
        if (v.isUndefined())
            return;
        if (v.isString() && choices.contains(v.toString()))
            target = v.toString();
        else
            warnInvalid(key);
    };

    readBool("firstRunCompleted", m_firstRunCompleted);
    readChoice("language", validLanguages(), m_language);
    readChoice("theme", validThemes(), m_theme);
    readChoice("defaultSort", validSortKeys(), m_defaultSort);
    readBool("reduceMotion", m_reduceMotion);
    readBool("fullscreen", m_fullscreen);
    readBool("sidebarCompact", m_sidebarCompact);

    const QJsonValue scale = obj.value(QLatin1String("interfaceScale"));
    if (scale.isDouble())
        m_interfaceScale = std::clamp(scale.toDouble(), MinScale, MaxScale);
    else if (!scale.isUndefined())
        warnInvalid("interfaceScale");

    const QJsonValue dirs = obj.value(QLatin1String("libraryDirectories"));
    if (dirs.isArray()) {
        // Directories that are currently missing (e.g. an unplugged drive) are
        // kept; the library scan reports them as skipped.
        for (const QJsonValue &v : dirs.toArray()) {
            const QString path = v.toString();
            if (!v.isString() || path.isEmpty() || QFileInfo(path).isRelative()) {
                warnInvalid("libraryDirectories[]");
                continue;
            }
            const QString clean = normalizedPath(path);
            if (!m_libraryDirectories.contains(clean))
                m_libraryDirectories.append(clean);
        }
    } else if (!dirs.isUndefined()) {
        warnInvalid("libraryDirectories");
    }
}

void AppSettings::save()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("firstRunCompleted"), m_firstRunCompleted);
    obj.insert(QStringLiteral("language"), m_language);
    obj.insert(QStringLiteral("theme"), m_theme);
    obj.insert(QStringLiteral("interfaceScale"), m_interfaceScale);
    obj.insert(QStringLiteral("defaultSort"), m_defaultSort);
    obj.insert(QStringLiteral("reduceMotion"), m_reduceMotion);
    obj.insert(QStringLiteral("fullscreen"), m_fullscreen);
    obj.insert(QStringLiteral("sidebarCompact"), m_sidebarCompact);
    obj.insert(QStringLiteral("libraryDirectories"), QJsonArray::fromStringList(m_libraryDirectories));
    QString error;
    if (!m_store.save(obj, &error))
        qWarning("Settings: %s", qPrintable(error));
}

bool AppSettings::firstRunCompleted() const { return m_firstRunCompleted; }
QString AppSettings::language() const { return m_language; }
QString AppSettings::theme() const { return m_theme; }
qreal AppSettings::interfaceScale() const { return m_interfaceScale; }
QString AppSettings::defaultSort() const { return m_defaultSort; }
bool AppSettings::reduceMotion() const { return m_reduceMotion; }
bool AppSettings::fullscreen() const { return m_fullscreen; }
bool AppSettings::sidebarCompact() const { return m_sidebarCompact; }
QStringList AppSettings::libraryDirectories() const { return m_libraryDirectories; }
QString AppSettings::filePath() const { return m_store.filePath(); }
QString AppSettings::loadError() const { return m_loadError; }

#define NEXORA_SETTER(member, value) \
    if (member == value)             \
        return;                      \
    member = value;                  \
    save();                          \
    emit changed();

void AppSettings::setFirstRunCompleted(bool value) { NEXORA_SETTER(m_firstRunCompleted, value) }
void AppSettings::setReduceMotion(bool value) { NEXORA_SETTER(m_reduceMotion, value) }
void AppSettings::setFullscreen(bool value) { NEXORA_SETTER(m_fullscreen, value) }
void AppSettings::setSidebarCompact(bool value) { NEXORA_SETTER(m_sidebarCompact, value) }

void AppSettings::setLanguage(const QString &value)
{
    if (!validLanguages().contains(value)) {
        qWarning("Settings: unsupported language \"%s\"", qPrintable(value));
        return;
    }
    NEXORA_SETTER(m_language, value)
}

void AppSettings::setTheme(const QString &value)
{
    if (!validThemes().contains(value)) {
        qWarning("Settings: unsupported theme \"%s\"", qPrintable(value));
        return;
    }
    NEXORA_SETTER(m_theme, value)
}

void AppSettings::setDefaultSort(const QString &value)
{
    if (!validSortKeys().contains(value)) {
        qWarning("Settings: unsupported sort key \"%s\"", qPrintable(value));
        return;
    }
    NEXORA_SETTER(m_defaultSort, value)
}

void AppSettings::setInterfaceScale(qreal value)
{
    const qreal clamped = std::clamp(value, MinScale, MaxScale);
    if (qFuzzyCompare(m_interfaceScale, clamped))
        return;
    m_interfaceScale = clamped;
    save();
    emit changed();
}

#undef NEXORA_SETTER

QString AppSettings::addLibraryDirectory(const QString &path)
{
    if (path.trimmed().isEmpty())
        return QStringLiteral("No directory selected");
    const QString clean = normalizedPath(path);
    const QFileInfo info(clean);
    if (!info.exists())
        return QStringLiteral("Directory does not exist");
    if (!info.isDir())
        return QStringLiteral("Not a directory");
    if (!info.isReadable() || !info.isExecutable())
        return QStringLiteral("Directory is not readable");
    if (m_libraryDirectories.contains(clean))
        return QStringLiteral("Already configured");
    m_libraryDirectories.append(clean);
    save();
    emit changed();
    emit libraryDirectoriesChanged();
    return {};
}

bool AppSettings::removeLibraryDirectory(const QString &path)
{
    if (path.isEmpty() || !m_libraryDirectories.removeOne(normalizedPath(path)))
        return false;
    save();
    emit changed();
    emit libraryDirectoriesChanged();
    return true;
}

void AppSettings::restoreDefaults(bool includeLibrary)
{
    m_language = QStringLiteral("en");
    m_theme = QStringLiteral("dark");
    m_interfaceScale = 1.0;
    m_defaultSort = QStringLiteral("title-asc");
    m_reduceMotion = false;
    m_fullscreen = true;
    m_sidebarCompact = false;
    const bool libraryChanged = includeLibrary && !m_libraryDirectories.isEmpty();
    if (includeLibrary) {
        m_firstRunCompleted = false;
        m_libraryDirectories.clear();
    }
    save();
    emit changed();
    if (libraryChanged)
        emit libraryDirectoriesChanged();
}

} // namespace nexora
