#include "directorybrowser.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>

namespace nexora {

DirectoryBrowser::DirectoryBrowser(QObject *parent)
    : QObject(parent)
{
    home();
}

QString DirectoryBrowser::currentPath() const { return m_path; }
QStringList DirectoryBrowser::entries() const { return m_entries; }
bool DirectoryBrowser::canGoUp() const { return !m_path.isEmpty() && !QDir(m_path).isRoot(); }
QString DirectoryBrowser::error() const { return m_error; }

QVariantList DirectoryBrowser::locations() const
{
    const QString home = QDir::homePath();
    QString user = qEnvironmentVariable("USER");
    if (user.isEmpty())
        user = QFileInfo(home).fileName();
    const QList<QPair<QString, QString>> candidates{
        {QStringLiteral("Home"), home},
        {QStringLiteral("Media"), QStringLiteral("/media")},
        {QStringLiteral("Mounts"), QStringLiteral("/mnt")},
        {QStringLiteral("Removable drives"), user.isEmpty() ? QString() : QStringLiteral("/run/media/") + user},
        {QStringLiteral("File system"), QStringLiteral("/")},
    };
    QVariantList result;
    for (const auto &candidate : candidates) {
        if (!candidate.second.isEmpty() && QFileInfo(candidate.second).isDir())
            result.append(QVariantMap{{QStringLiteral("name"), candidate.first}, {QStringLiteral("path"), candidate.second}});
    }
    return result;
}

bool DirectoryBrowser::open(const QString &path)
{
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QFileInfo info(clean);
    QString error;
    if (path.isEmpty() || !info.exists())
        error = QStringLiteral("Folder not found");
    else if (!info.isDir())
        error = QStringLiteral("Not a folder");
    else if (!info.isReadable() || !info.isExecutable())
        error = QStringLiteral("Permission denied");

    if (!error.isEmpty()) {
        m_error = error;
        emit currentPathChanged();
        return false;
    }

    QStringList names = QDir(clean).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        const int c = QString::localeAwareCompare(a.toLower(), b.toLower());
        return c != 0 ? c < 0 : a < b;
    });
    m_path = clean;
    m_entries = names;
    m_error.clear();
    emit currentPathChanged();
    return true;
}

bool DirectoryBrowser::enter(const QString &name)
{
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QLatin1String("..")
        || name == QLatin1String(".")) {
        m_error = QStringLiteral("Folder not found");
        emit currentPathChanged();
        return false;
    }
    return open(QDir(m_path).filePath(name));
}

bool DirectoryBrowser::up()
{
    if (!canGoUp())
        return false;
    return open(QFileInfo(m_path).absolutePath());
}

void DirectoryBrowser::home()
{
    if (!open(QDir::homePath()))
        open(QDir::rootPath());
}

} // namespace nexora
