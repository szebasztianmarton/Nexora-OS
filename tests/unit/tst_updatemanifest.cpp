#include "nexora/update/UpdateManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using nexora::update::UpdateManifest;

namespace {

QJsonObject validRoot()
{
    return QJsonObject{{QStringLiteral("format"), 1},
                       {QStringLiteral("product"), QStringLiteral("nexora-os")},
                       {QStringLiteral("board"), QStringLiteral("nexora-x86_64")},
                       {QStringLiteral("version"), QStringLiteral("0.4.1")},
                       {QStringLiteral("buildId"), QStringLiteral("v0.4.1-0-gabcdef0")},
                       {QStringLiteral("minimumVersion"), QStringLiteral("0.4.0")},
                       {QStringLiteral("releasedAt"), QStringLiteral("2026-10-11T00:00:00Z")},
                       {QStringLiteral("image"),
                        QJsonObject{{QStringLiteral("file"), QStringLiteral("rootfs.ext4")},
                                    {QStringLiteral("size"), 2147483648.0},
                                    {QStringLiteral("sha256"), QString(64, QLatin1Char('a'))}}},
                       {QStringLiteral("notes"), QStringLiteral("release notes")}};
}

QByteArray toJson(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QJsonObject withImage(QJsonObject root, const QString &key, const QJsonValue &value)
{
    QJsonObject image = root.value(QStringLiteral("image")).toObject();
    image[key] = value;
    root[QStringLiteral("image")] = image;
    return root;
}

} // namespace

class TestUpdateManifest : public QObject
{
    Q_OBJECT
private slots:
    void parsesValidManifest()
    {
        QString error;
        const auto m = UpdateManifest::parse(toJson(validRoot()), &error);
        QVERIFY2(m.has_value(), qPrintable(error));
        QCOMPARE(m->format, 1);
        QCOMPARE(m->product, QStringLiteral("nexora-os"));
        QCOMPARE(m->board, QStringLiteral("nexora-x86_64"));
        QCOMPARE(m->buildId, QStringLiteral("v0.4.1-0-gabcdef0"));
        QCOMPARE(m->version.toString(), QStringLiteral("0.4.1"));
        QCOMPARE(m->minimumVersion.toString(), QStringLiteral("0.4.0"));
        QVERIFY(m->releasedAt.isValid());
        QCOMPARE(m->image.file, QStringLiteral("rootfs.ext4"));
        QCOMPARE(m->image.size, qint64(2147483648));
        QCOMPARE(m->image.sha256Hex, QByteArray(64, 'a'));
        QCOMPARE(m->notes, QStringLiteral("release notes"));
    }

    void parsesWithoutOptionalNotes()
    {
        QJsonObject root = validRoot();
        root.remove(QStringLiteral("notes"));
        QString error;
        const auto m = UpdateManifest::parse(toJson(root), &error);
        QVERIFY2(m.has_value(), qPrintable(error));
        QVERIFY(m->notes.isEmpty());
    }

    void rejectsUnknownTopLevelKey()
    {
        QJsonObject root = validRoot();
        root[QStringLiteral("bogus")] = QStringLiteral("x");
        QString error;
        QVERIFY(!UpdateManifest::parse(toJson(root), &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void rejectsMissingImage()
    {
        QJsonObject root = validRoot();
        root.remove(QStringLiteral("image"));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsImageFileWithParentReference()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("file"), QStringLiteral("../x"));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsImageFileWithSlash()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("file"), QStringLiteral("a/b"));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsOverlongImageFileName()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("file"), QString(129, QLatin1Char('a')));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsUppercaseHash()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("sha256"), QString(64, QLatin1Char('A')));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsShortHash()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("sha256"), QString(63, QLatin1Char('a')));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsZeroSize()
    {
        const QJsonObject root = withImage(validRoot(), QStringLiteral("size"), 0);
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsOversizeImage()
    {
        const QJsonObject root
            = withImage(validRoot(), QStringLiteral("size"), double(qint64(65) * 1024 * 1024 * 1024));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsUnsupportedFormat()
    {
        QJsonObject root = validRoot();
        root[QStringLiteral("format")] = 2;
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsOverlongNotes()
    {
        QJsonObject root = validRoot();
        root[QStringLiteral("notes")] = QString(4001, QLatin1Char('x'));
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }

    void rejectsNonObjectRoot()
    {
        const QByteArray json = QJsonDocument(QJsonArray{1, 2, 3}).toJson(QJsonDocument::Compact);
        QVERIFY(!UpdateManifest::parse(json, nullptr).has_value());
    }

    void rejectsInputOver64KiB()
    {
        QJsonObject root = validRoot();
        root[QStringLiteral("notes")] = QString(70 * 1024, QLatin1Char('x'));
        const QByteArray json = toJson(root);
        QVERIFY(json.size() > 64 * 1024);
        QVERIFY(!UpdateManifest::parse(json, nullptr).has_value());
    }

    void rejectsInvalidVersion()
    {
        QJsonObject root = validRoot();
        root[QStringLiteral("version")] = QStringLiteral("v0.4.1");
        QVERIFY(!UpdateManifest::parse(toJson(root), nullptr).has_value());
    }
};

QTEST_APPLESS_MAIN(TestUpdateManifest)
#include "tst_updatemanifest.moc"
