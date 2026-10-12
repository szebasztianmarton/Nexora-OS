#include "UpdateBundleFixture.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include <openssl/evp.h>
#include <openssl/pem.h>

namespace nexora::update::testsupport {

namespace {
EVP_PKEY *asPkey(void *key)
{
    return reinterpret_cast<EVP_PKEY *>(key);
}
} // namespace

TestKeyPair TestKeyPair::generate()
{
    TestKeyPair kp;
    kp.m_pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "ED25519");
    return kp;
}

TestKeyPair::~TestKeyPair()
{
    EVP_PKEY_free(asPkey(m_pkey));
}

TestKeyPair::TestKeyPair(TestKeyPair &&other) noexcept
    : m_pkey(other.m_pkey)
{
    other.m_pkey = nullptr;
}

TestKeyPair &TestKeyPair::operator=(TestKeyPair &&other) noexcept
{
    if (this != &other) {
        EVP_PKEY_free(asPkey(m_pkey));
        m_pkey = other.m_pkey;
        other.m_pkey = nullptr;
    }
    return *this;
}

QByteArray TestKeyPair::publicKeyPem() const
{
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, asPkey(m_pkey));
    char *data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    const QByteArray pem(data, int(len));
    BIO_free(bio);
    return pem;
}

QByteArray TestKeyPair::sign(const QByteArray &message) const
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, asPkey(m_pkey));
    size_t sigLen = 0;
    EVP_DigestSign(ctx, nullptr, &sigLen, reinterpret_cast<const unsigned char *>(message.constData()),
                   size_t(message.size()));
    QByteArray signature(int(sigLen), '\0');
    EVP_DigestSign(ctx, reinterpret_cast<unsigned char *>(signature.data()), &sigLen,
                   reinterpret_cast<const unsigned char *>(message.constData()), size_t(message.size()));
    signature.resize(int(sigLen));
    EVP_MD_CTX_free(ctx);
    return signature;
}

BuiltBundle writeBundle(const QString &bundleDir, const BundleSpec &spec, const TestKeyPair &key)
{
    QDir().mkpath(bundleDir);
    const QString imagePath = bundleDir + u'/' + spec.imageFileName;

    {
        QFile image(imagePath);
        image.open(QIODevice::WriteOnly | QIODevice::Truncate);
        if (spec.sparseImage) {
            image.resize(spec.imageSize);
        } else {
            // Deterministic pseudo-random fill: a one-byte tamper is then detectable, and
            // the content isn't all-zero (which some tools/filesystems special-case).
            constexpr qint64 chunkSize = 1024 * 1024;
            QByteArray chunk(int(std::min<qint64>(chunkSize, spec.imageSize)), '\0');
            quint32 seed = 0x9e3779b9u;
            qint64 written = 0;
            while (written < spec.imageSize) {
                const qint64 take = std::min<qint64>(chunk.size(), spec.imageSize - written);
                for (qint64 i = 0; i < take; ++i) {
                    seed = seed * 1103515245u + 12345u;
                    chunk[int(i)] = char(seed >> 24);
                }
                image.write(chunk.constData(), take);
                written += take;
            }
        }
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    {
        QFile forHash(imagePath);
        forHash.open(QIODevice::ReadOnly);
        hash.addData(&forHash);
    }
    const QByteArray sha256Hex = hash.result().toHex();

    const QJsonObject imageObj{{QStringLiteral("file"), spec.imageFileName},
                               {QStringLiteral("size"), double(spec.imageSize)},
                               {QStringLiteral("sha256"), QString::fromLatin1(sha256Hex)}};
    const QJsonObject root{{QStringLiteral("format"), 1},
                           {QStringLiteral("product"), spec.product},
                           {QStringLiteral("board"), spec.board},
                           {QStringLiteral("version"), spec.version},
                           {QStringLiteral("buildId"), spec.buildId},
                           {QStringLiteral("minimumVersion"), spec.minimumVersion},
                           {QStringLiteral("releasedAt"), spec.releasedAt},
                           {QStringLiteral("image"), imageObj},
                           {QStringLiteral("notes"), spec.notes}};
    const QByteArray manifestJson = QJsonDocument(root).toJson(QJsonDocument::Compact);

    QFile manifestFile(bundleDir + QStringLiteral("/manifest.json"));
    manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    manifestFile.write(manifestJson);
    manifestFile.close();

    const QByteArray signature = key.sign(manifestJson);
    QFile sigFile(bundleDir + QStringLiteral("/manifest.json.sig"));
    sigFile.open(QIODevice::WriteOnly | QIODevice::Truncate);
    sigFile.write(signature);
    sigFile.close();

    return {bundleDir, manifestJson, sha256Hex, spec.imageSize};
}

void writePublicKey(const QString &keyDir, const QString &name, const TestKeyPair &key)
{
    QDir().mkpath(keyDir);
    QFile f(keyDir + u'/' + name + QStringLiteral(".pem"));
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(key.publicKeyPem());
}

} // namespace nexora::update::testsupport
