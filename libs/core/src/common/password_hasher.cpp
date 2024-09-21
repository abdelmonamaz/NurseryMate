#include "common/password_hasher.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace nursera {
namespace {

constexpr int kIterations = 120000;
constexpr int kSaltBytes = 16;
constexpr int kKeyBytes = 32;
const QString kPrefix = QStringLiteral("pbkdf2-sha256");

QByteArray randomSalt()
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    // CSPRNG système (doc 02 §8)
    QRandomGenerator::system()->fillRange(
        reinterpret_cast<quint32*>(salt.data()),
        static_cast<qsizetype>(kSaltBytes / sizeof(quint32)));
    return salt;
}

bool constantTimeEquals(const QByteArray& a, const QByteArray& b)
{
    if (a.size() != b.size())
        return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i)) ^ static_cast<unsigned char>(b.at(i));
    return diff == 0;
}

} // namespace

QString PasswordHasher::hash(const QString& secret)
{
    const QByteArray salt = randomSalt();
    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, secret.toUtf8(), salt, kIterations, kKeyBytes);
    return kPrefix + QLatin1Char('$') + QString::number(kIterations)
        + QLatin1Char('$') + QString::fromLatin1(salt.toBase64())
        + QLatin1Char('$') + QString::fromLatin1(key.toBase64());
}

bool PasswordHasher::verify(const QString& secret, const QString& stored)
{
    const QStringList parts = stored.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != kPrefix)
        return false;

    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0)
        return false;

    const QByteArray salt = QByteArray::fromBase64(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromBase64(parts.at(3).toLatin1());
    if (salt.isEmpty() || expected.isEmpty())
        return false;

    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, secret.toUtf8(), salt, iterations,
        static_cast<quint64>(expected.size()));
    return constantTimeEquals(key, expected);
}

} // namespace nursera
