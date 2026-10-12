#pragma once

#include <QByteArray>
#include <QString>

// Reusable helper for building signed update bundles in tests: a fresh Ed25519 keypair
// (OpenSSL EVP_PKEY_Q_keygen; no private key is ever written to disk or committed), a
// manifest.json + manifest.json.sig signed by it, and a root filesystem image. Shared by
// tst_signatureverifier, tst_updateservice and test_update_tool.sh (Task 5), and reusable
// by the Buildroot update-helper tests (Task 10) so bundle/key generation lives in one
// place.
namespace nexora::update::testsupport {

class TestKeyPair
{
public:
    static TestKeyPair generate();

    ~TestKeyPair();
    TestKeyPair(TestKeyPair &&other) noexcept;
    TestKeyPair &operator=(TestKeyPair &&other) noexcept;
    TestKeyPair(const TestKeyPair &) = delete;
    TestKeyPair &operator=(const TestKeyPair &) = delete;

    QByteArray publicKeyPem() const;
    QByteArray sign(const QByteArray &message) const;

private:
    TestKeyPair() = default;
    void *m_pkey = nullptr;
};

struct BundleSpec
{
    QString product = QStringLiteral("nexora-os");
    QString board = QStringLiteral("nexora-x86_64");
    QString version = QStringLiteral("0.4.1");
    QString minimumVersion = QStringLiteral("0.4.0");
    QString buildId = QStringLiteral("v0.4.1-0-gabcdef0");
    QString releasedAt = QStringLiteral("2026-10-11T00:00:00Z");
    QString notes = QStringLiteral("test bundle");
    QString imageFileName = QStringLiteral("rootfs.ext4");
    qint64 imageSize = 3 * 1024 * 1024; // 3 MiB
    // true: the image file is created at imageSize without writing content (fast; for
    // large-image / cancellation tests where the bytes themselves do not matter).
    bool sparseImage = false;
};

struct BuiltBundle
{
    QString dir;
    QByteArray manifestJson;
    QByteArray imageSha256Hex;
    qint64 imageSize = 0;
};

// Writes manifest.json, manifest.json.sig (signed by |key|) and the image file into
// |bundleDir| (created if needed, overwriting any existing files of the same name).
BuiltBundle writeBundle(const QString &bundleDir, const BundleSpec &spec, const TestKeyPair &key);

// Writes |key|'s public key to <keyDir>/<name>.pem (creating keyDir if needed).
void writePublicKey(const QString &keyDir, const QString &name, const TestKeyPair &key);

} // namespace nexora::update::testsupport
