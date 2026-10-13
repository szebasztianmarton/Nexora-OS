#include "nexora/update/SignatureVerifier.h"

#include "nexora/shared/AtomicFile.h"

#include <QDir>
#include <QFile>

#include <openssl/evp.h>
#include <openssl/pem.h>

namespace nexora::update {

namespace {

constexpr qint64 MaxKeyFileBytes = 16 * 1024;
constexpr qint64 ChunkBytes = 1024 * 1024;

EVP_PKEY *asPkey(void *key)
{
    return reinterpret_cast<EVP_PKEY *>(key);
}

} // namespace

void SignatureVerifier::KeyDeleter::operator()(void *key) const noexcept
{
    EVP_PKEY_free(asPkey(key));
}

SignatureVerifier SignatureVerifier::fromDirectory(const QString &dir, QStringList *issues)
{
    SignatureVerifier verifier;
    auto report = [&](const QString &message) {
        if (issues)
            issues->append(message);
    };
    const QDir keyDir(dir);
    if (!keyDir.exists()) {
        report(QStringLiteral("%1: no such directory").arg(dir));
        return verifier;
    }
    const QStringList files = keyDir.entryList({QStringLiteral("*.pem")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        QString readError;
        const auto pem = shared::readFileLimited(keyDir.filePath(name), MaxKeyFileBytes, &readError);
        if (!pem) {
            report(QStringLiteral("%1: %2").arg(name, readError));
            continue;
        }
        QString addError;
        verifier.addPublicKeyPem(*pem, &addError);
        if (!addError.isEmpty())
            report(QStringLiteral("%1: %2").arg(name, addError));
    }
    if (verifier.keyCount() == 0)
        report(QStringLiteral("no usable Ed25519 public keys in %1").arg(dir));
    return verifier;
}

void SignatureVerifier::addPublicKeyPem(const QByteArray &pem, QString *error)
{
    auto fail = [&](const QString &message) {
        if (error)
            *error = message;
    };
    BIO *bio = BIO_new_mem_buf(pem.constData(), int(pem.size()));
    if (!bio) {
        fail(QStringLiteral("out of memory"));
        return;
    }
    EVP_PKEY *pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        fail(QStringLiteral("not a valid PEM public key"));
        return;
    }
    if (EVP_PKEY_id(pkey) != EVP_PKEY_ED25519) {
        fail(QStringLiteral("key is not Ed25519"));
        EVP_PKEY_free(pkey);
        return;
    }
    m_keys.emplace_back(pkey, KeyDeleter{});
}

bool SignatureVerifier::verify(const QByteArray &message, const QByteArray &signature) const
{
    if (signature.size() != 64 || m_keys.empty())
        return false;
    for (const auto &key : m_keys) {
        EVP_MD_CTX *ctx = EVP_MD_CTX_new();
        if (!ctx)
            continue;
        const bool ok = EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, asPkey(key.get())) == 1
            && EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(signature.constData()),
                                size_t(signature.size()), reinterpret_cast<const unsigned char *>(message.constData()),
                                size_t(message.size()))
                == 1;
        EVP_MD_CTX_free(ctx);
        if (ok)
            return true;
    }
    return false;
}

std::optional<QByteArray> sha256File(const QString &path, const std::function<void(qint64, qint64)> &progress,
                                     const std::atomic_bool *cancel, QString *error)
{
    auto fail = [&](const QString &message) -> std::optional<QByteArray> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("cannot open %1").arg(path));
    const qint64 total = file.size();

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return fail(QStringLiteral("cannot initialise SHA-256"));
    }

    qint64 done = 0;
    QByteArray buffer;
    buffer.resize(ChunkBytes);
    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(buffer.data(), buffer.size());
        if (bytesRead < 0) {
            EVP_MD_CTX_free(ctx);
            return fail(QStringLiteral("read error on %1").arg(path));
        }
        if (bytesRead > 0 && EVP_DigestUpdate(ctx, buffer.constData(), size_t(bytesRead)) != 1) {
            EVP_MD_CTX_free(ctx);
            return fail(QStringLiteral("hashing failed"));
        }
        done += bytesRead;
        if (progress)
            progress(done, total);
        if (cancel && cancel->load()) {
            EVP_MD_CTX_free(ctx);
            return fail(QStringLiteral("cancelled"));
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    const bool ok = EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok)
        return fail(QStringLiteral("hashing failed"));
    return QByteArray(reinterpret_cast<const char *>(digest), int(digestLen)).toHex();
}

} // namespace nexora::update
