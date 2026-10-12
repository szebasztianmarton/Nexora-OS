#include "nexora/update/SignatureVerifier.h"

#include "UpdateBundleFixture.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <atomic>

#include <openssl/evp.h>
#include <openssl/pem.h>

using nexora::update::sha256File;
using nexora::update::SignatureVerifier;
using nexora::update::testsupport::TestKeyPair;

namespace {

// An RSA public key, used only to confirm non-Ed25519 keys are rejected.
QByteArray rsaPublicKeyPem()
{
    EVP_PKEY *pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "RSA", size_t(2048));
    BIO *bio = BIO_new(BIO_s_mem());
    PEM_write_bio_PUBKEY(bio, pkey);
    char *data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    const QByteArray pem(data, int(len));
    BIO_free(bio);
    EVP_PKEY_free(pkey);
    return pem;
}

void writeFile(const QString &path, const QByteArray &content)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(content);
}

} // namespace

class TestSignatureVerifier : public QObject
{
    Q_OBJECT
private slots:
    void verifiesASignedMessage()
    {
        const auto key = TestKeyPair::generate();
        SignatureVerifier verifier;
        QString error;
        verifier.addPublicKeyPem(key.publicKeyPem(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(verifier.keyCount(), 1);
        const QByteArray message = "hello update";
        const QByteArray signature = key.sign(message);
        QCOMPARE(signature.size(), 64);
        QVERIFY(verifier.verify(message, signature));
    }

    void rejectsATamperedMessage()
    {
        const auto key = TestKeyPair::generate();
        SignatureVerifier verifier;
        verifier.addPublicKeyPem(key.publicKeyPem(), nullptr);
        QByteArray message = "hello update";
        const QByteArray signature = key.sign(message);
        message[0] = char(message[0] ^ 0x1);
        QVERIFY(!verifier.verify(message, signature));
    }

    void rejectsATamperedSignature()
    {
        const auto key = TestKeyPair::generate();
        SignatureVerifier verifier;
        verifier.addPublicKeyPem(key.publicKeyPem(), nullptr);
        const QByteArray message = "hello update";
        QByteArray signature = key.sign(message);
        signature[0] = char(signature[0] ^ 0x1);
        QVERIFY(!verifier.verify(message, signature));
    }

    void failsClosedWithZeroKeys()
    {
        SignatureVerifier verifier;
        QCOMPARE(verifier.keyCount(), 0);
        QVERIFY(!verifier.verify("hello", QByteArray(64, '\0')));
    }

    void rejectsWrongSizedSignature()
    {
        const auto key = TestKeyPair::generate();
        SignatureVerifier verifier;
        verifier.addPublicKeyPem(key.publicKeyPem(), nullptr);
        QVERIFY(!verifier.verify("hello", QByteArray(63, '\0')));
    }

    void rejectsRsaKeyWithIssue()
    {
        SignatureVerifier verifier;
        QString error;
        verifier.addPublicKeyPem(rsaPublicKeyPem(), &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(verifier.keyCount(), 0);
    }

    void verifiesWithTheSecondOfTwoKeys()
    {
        const auto key1 = TestKeyPair::generate();
        const auto key2 = TestKeyPair::generate();
        SignatureVerifier verifier;
        verifier.addPublicKeyPem(key1.publicKeyPem(), nullptr);
        verifier.addPublicKeyPem(key2.publicKeyPem(), nullptr);
        QCOMPARE(verifier.keyCount(), 2);
        const QByteArray message = "signed by the second key";
        const QByteArray signature = key2.sign(message);
        QVERIFY(verifier.verify(message, signature));
    }

    void fromDirectoryLoadsPemFilesAndReportsIssues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto key = TestKeyPair::generate();
        writeFile(dir.filePath(QStringLiteral("good.pem")), key.publicKeyPem());
        writeFile(dir.filePath(QStringLiteral("bad.pem")), "not a pem file");
        writeFile(dir.filePath(QStringLiteral("ignored.txt")), "not even considered");

        QStringList issues;
        const auto verifier = SignatureVerifier::fromDirectory(dir.path(), &issues);
        QCOMPARE(verifier.keyCount(), 1);
        QVERIFY(!issues.isEmpty());
    }

    void fromDirectoryFailsClosedWithNoKeys()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QStringList issues;
        const auto verifier = SignatureVerifier::fromDirectory(dir.path(), &issues);
        QCOMPARE(verifier.keyCount(), 0);
        QVERIFY(!verifier.verify("x", QByteArray(64, '\0')));
        QVERIFY(!issues.isEmpty());
    }

    void sha256FileMatchesKnownDigest()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("data.bin"));
        writeFile(path, "abc");
        QString error;
        const auto digest = sha256File(path, {}, nullptr, &error);
        QVERIFY2(digest.has_value(), qPrintable(error));
        QCOMPARE(*digest, QByteArray("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    }

    void sha256FileReportsProgressPerChunk()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("data.bin"));
        writeFile(path, QByteArray(3 * 1024 * 1024, 'x')); // exactly 3 one-MiB chunks
        QList<qint64> doneValues;
        const auto digest = sha256File(
            path,
            [&](qint64 done, qint64 total) {
                doneValues.append(done);
                QCOMPARE(total, qint64(3 * 1024 * 1024));
            },
            nullptr, nullptr);
        QVERIFY(digest.has_value());
        QCOMPARE(doneValues.size(), 3);
        QCOMPARE(doneValues.last(), qint64(3 * 1024 * 1024));
    }

    void sha256FileCancelsDeterministically()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("data.bin"));
        writeFile(path, QByteArray(5 * 1024 * 1024, 'y'));
        std::atomic_bool cancel{false};
        int chunks = 0;
        QString error;
        const auto digest = sha256File(
            path,
            [&](qint64, qint64) {
                // Deterministic: the cancellation flag is set from inside the very
                // callback the hashing loop itself calls, so it is observed on the next
                // chunk check regardless of timing (same technique as
                // tst_libraryscanner's cancelDuringTheWalkKeepsAPartialResult()).
                if (++chunks == 1)
                    cancel.store(true);
            },
            &cancel, &error);
        QVERIFY(!digest.has_value());
        QCOMPARE(error, QStringLiteral("cancelled"));
        QCOMPARE(chunks, 1); // the second chunk is never hashed
    }
};

QTEST_APPLESS_MAIN(TestSignatureVerifier)
#include "tst_signatureverifier.moc"
