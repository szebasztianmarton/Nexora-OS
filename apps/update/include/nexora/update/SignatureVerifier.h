#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace nexora::update {

// Verifies Ed25519 signatures against a fixed set of public keys loaded from PEM files.
// Fails closed: with zero keys loaded, verify() always returns false. See the trust model
// in docs/update-system.md.
class SignatureVerifier
{
public:
    SignatureVerifier() = default;
    SignatureVerifier(SignatureVerifier &&) noexcept = default;
    SignatureVerifier &operator=(SignatureVerifier &&) noexcept = default;
    SignatureVerifier(const SignatureVerifier &) = delete;
    SignatureVerifier &operator=(const SignatureVerifier &) = delete;

    // Loads every "*.pem" file directly in |dir|. A file that cannot be read, is not a
    // valid PEM public key, or whose key is not Ed25519, is skipped and reported in
    // |issues| (a missing directory or zero usable keys are reported there too); the
    // returned verifier simply has fewer keys, down to zero.
    static SignatureVerifier fromDirectory(const QString &dir, QStringList *issues);

    // Adds one Ed25519 public key. Sets |error| and leaves the verifier unchanged if |pem|
    // is not a valid PEM public key or is not Ed25519.
    void addPublicKeyPem(const QByteArray &pem, QString *error);

    int keyCount() const { return int(m_keys.size()); }

    // True if |signature| (must be exactly 64 bytes) is a valid Ed25519 signature over
    // |message| by any loaded key.
    bool verify(const QByteArray &message, const QByteArray &signature) const;

private:
    struct KeyDeleter
    {
        void operator()(void *key) const noexcept;
    };
    std::vector<std::unique_ptr<void, KeyDeleter>> m_keys;
};

// Streaming SHA-256 of a file, read in 1 MiB chunks. |progress| (if set) is called after
// every chunk with the bytes hashed so far and the file size. |cancel| (if set) is checked
// after every chunk; a cancellation returns nullopt with |error| set to "cancelled".
std::optional<QByteArray> sha256File(const QString &path,
                                     const std::function<void(qint64 done, qint64 total)> &progress,
                                     const std::atomic_bool *cancel, QString *error);

} // namespace nexora::update
