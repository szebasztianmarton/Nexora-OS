#include "nexora/update/UpdateManifest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace nexora::update {

namespace {

constexpr qint64 MaxManifestBytes = 64 * 1024;
constexpr qint64 MaxNotesChars = 4000;
constexpr qint64 MaxFileNameChars = 128;
constexpr qint64 MinImageBytes = 1;
constexpr qint64 MaxImageBytes = qint64(64) * 1024 * 1024 * 1024;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

const QRegularExpression &fileNamePattern()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9._-]+$"));
    return re;
}

const QRegularExpression &sha256Pattern()
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{64}$"));
    return re;
}

std::optional<ImageInfo> parseImage(const QJsonValue &value, QString *error)
{
    if (!value.isObject()) {
        fail(error, QStringLiteral("\"image\" must be an object"));
        return std::nullopt;
    }
    const QJsonObject obj = value.toObject();
    static const QSet<QString> allowed{QStringLiteral("file"), QStringLiteral("size"), QStringLiteral("sha256")};
    const QStringList keys = obj.keys();
    for (const QString &key : keys) {
        if (!allowed.contains(key)) {
            fail(error, QStringLiteral("unknown key \"%1\" in \"image\"").arg(key));
            return std::nullopt;
        }
    }

    ImageInfo image;
    const QJsonValue fileValue = obj.value(QStringLiteral("file"));
    if (!fileValue.isString()) {
        fail(error, QStringLiteral("\"image.file\" must be a string"));
        return std::nullopt;
    }
    image.file = fileValue.toString();
    if (image.file.isEmpty() || image.file.size() > MaxFileNameChars || image.file.contains(u'/')
        || image.file.contains(QStringLiteral("..")) || !fileNamePattern().match(image.file).hasMatch()) {
        fail(error, QStringLiteral("\"image.file\" must be a plain file name"));
        return std::nullopt;
    }

    const QJsonValue sizeValue = obj.value(QStringLiteral("size"));
    if (!sizeValue.isDouble()) {
        fail(error, QStringLiteral("\"image.size\" must be a number"));
        return std::nullopt;
    }
    const double rawSize = sizeValue.toDouble();
    if (rawSize != std::floor(rawSize) || rawSize < double(MinImageBytes) || rawSize > double(MaxImageBytes)) {
        fail(error, QStringLiteral("\"image.size\" must be an integer between 1 and 64 GiB"));
        return std::nullopt;
    }
    image.size = qint64(rawSize);

    const QJsonValue shaValue = obj.value(QStringLiteral("sha256"));
    if (!shaValue.isString()) {
        fail(error, QStringLiteral("\"image.sha256\" must be a string"));
        return std::nullopt;
    }
    const QString sha = shaValue.toString();
    if (!sha256Pattern().match(sha).hasMatch()) {
        fail(error, QStringLiteral("\"image.sha256\" must be 64 lowercase hex characters"));
        return std::nullopt;
    }
    image.sha256Hex = sha.toLatin1();
    return image;
}

} // namespace

std::optional<UpdateManifest> UpdateManifest::parse(const QByteArray &json, QString *error)
{
    if (json.size() > MaxManifestBytes) {
        fail(error, QStringLiteral("manifest exceeds 64 KiB"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        fail(error, QStringLiteral("manifest is not a valid JSON object"));
        return std::nullopt;
    }
    const QJsonObject root = doc.object();

    static const QSet<QString> allowedKeys{
        QStringLiteral("format"),     QStringLiteral("product"), QStringLiteral("board"),
        QStringLiteral("version"),    QStringLiteral("buildId"), QStringLiteral("minimumVersion"),
        QStringLiteral("releasedAt"), QStringLiteral("image"),   QStringLiteral("notes")};
    const QStringList keys = root.keys();
    for (const QString &key : keys) {
        if (!allowedKeys.contains(key)) {
            fail(error, QStringLiteral("unknown key \"%1\"").arg(key));
            return std::nullopt;
        }
    }

    UpdateManifest manifest;
    const QJsonValue formatValue = root.value(QStringLiteral("format"));
    if (!formatValue.isDouble() || formatValue.toInt(-1) != 1) {
        fail(error, QStringLiteral("unsupported manifest \"format\""));
        return std::nullopt;
    }
    manifest.format = 1;

    auto requireString = [&](const char *key, QString &out) {
        const QJsonValue value = root.value(QLatin1String(key));
        if (!value.isString() || value.toString().isEmpty()) {
            fail(error, QStringLiteral("\"%1\" must be a non-empty string").arg(QLatin1String(key)));
            return false;
        }
        out = value.toString();
        return true;
    };
    if (!requireString("product", manifest.product))
        return std::nullopt;
    if (!requireString("board", manifest.board))
        return std::nullopt;
    if (!requireString("buildId", manifest.buildId))
        return std::nullopt;

    QString versionText;
    if (!requireString("version", versionText))
        return std::nullopt;
    const auto version = Version::parse(versionText);
    if (!version) {
        fail(error, QStringLiteral("\"version\" is not a valid version"));
        return std::nullopt;
    }
    manifest.version = *version;

    QString minimumVersionText;
    if (!requireString("minimumVersion", minimumVersionText))
        return std::nullopt;
    const auto minimumVersion = Version::parse(minimumVersionText);
    if (!minimumVersion) {
        fail(error, QStringLiteral("\"minimumVersion\" is not a valid version"));
        return std::nullopt;
    }
    manifest.minimumVersion = *minimumVersion;

    QString releasedAtText;
    if (!requireString("releasedAt", releasedAtText))
        return std::nullopt;
    manifest.releasedAt = QDateTime::fromString(releasedAtText, Qt::ISODate);
    if (!manifest.releasedAt.isValid()) {
        fail(error, QStringLiteral("\"releasedAt\" is not a valid ISO-8601 date-time"));
        return std::nullopt;
    }

    if (!root.contains(QStringLiteral("image"))) {
        fail(error, QStringLiteral("missing \"image\""));
        return std::nullopt;
    }
    const auto image = parseImage(root.value(QStringLiteral("image")), error);
    if (!image)
        return std::nullopt;
    manifest.image = *image;

    if (root.contains(QStringLiteral("notes"))) {
        const QJsonValue notesValue = root.value(QStringLiteral("notes"));
        if (!notesValue.isString()) {
            fail(error, QStringLiteral("\"notes\" must be a string"));
            return std::nullopt;
        }
        manifest.notes = notesValue.toString();
        if (manifest.notes.size() > MaxNotesChars) {
            fail(error, QStringLiteral("\"notes\" exceeds 4000 characters"));
            return std::nullopt;
        }
    }

    return manifest;
}

} // namespace nexora::update
