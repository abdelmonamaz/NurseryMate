#include "controllers/auth_controller.h"

#include "common/password_hasher.h"

#include <QRegularExpression>

namespace nursera {
namespace {

QString roleCode(UserRole role)
{
    switch (role) {
    case UserRole::Manager: return QStringLiteral("manager");
    case UserRole::Seller: return QStringLiteral("seller");
    case UserRole::Worker: break;
    }
    return QStringLiteral("worker");
}

QString usernameFrom(const QString& displayName)
{
    QString username = displayName.toLower().simplified();
    username.replace(QLatin1Char(' '), QLatin1Char('.'));
    return username;
}

} // namespace

AuthController::AuthController(IUserRepository& users, QObject* parent)
    : QObject(parent)
    , m_users(users)
{
    refresh();
}

bool AuthController::isValidPin(const QString& pin)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9]{4,6}$"));
    return pattern.match(pin).hasMatch();
}

QVariantList AuthController::users() const
{
    QVariantList list;
    for (const User& user : m_userList) {
        list.append(QVariantMap{
            {QStringLiteral("id"), user.id},
            {QStringLiteral("name"), user.displayName},
            {QStringLiteral("role"), roleCode(user.role)},
        });
    }
    return list;
}

QString AuthController::currentRole() const
{
    return authenticated() ? roleCode(m_currentUser.role) : QString();
}

void AuthController::refresh()
{
    const auto total = m_users.totalCount();
    m_needsSetup = total.isOk() && total.value() == 0;

    const auto active = m_users.activeUsers();
    m_userList = active ? active.value() : QList<User>{};
    emit usersChanged();
}

bool AuthController::setupManager(const QString& displayName,
                                  const QString& pin,
                                  const QString& confirm)
{
    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }
    if (!isValidPin(pin)) {
        emit errorOccurred(tr("Le PIN doit contenir 4 à 6 chiffres."));
        return false;
    }
    if (pin != confirm) {
        emit errorOccurred(tr("Les deux PIN ne correspondent pas."));
        return false;
    }
    if (!m_needsSetup) {
        emit errorOccurred(tr("Un compte existe déjà."));
        return false;
    }

    User manager;
    manager.username = usernameFrom(name);
    manager.displayName = name;
    manager.role = UserRole::Manager;
    const auto created = m_users.insert(manager, PasswordHasher::hash(pin));
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }

    refresh();
    manager.id = created.value();
    m_currentUser = manager;
    emit sessionChanged();
    return true;
}

bool AuthController::login(int userId, const QString& pin)
{
    if (m_failedAttempts.value(userId, 0) >= kMaxAttempts) {
        emit loginFailed(tr("Compte verrouillé après %1 échecs — "
                            "demandez au gérant de redémarrer l'application.")
                             .arg(kMaxAttempts));
        return false;
    }

    const auto storedHash = m_users.pinHashOf(userId);
    if (!storedHash) {
        emit loginFailed(storedHash.error().message);
        return false;
    }

    if (!PasswordHasher::verify(pin, storedHash.value())) {
        const int attempts = ++m_failedAttempts[userId];
        emit loginFailed(tr("PIN incorrect (essai %1/%2).")
                             .arg(attempts)
                             .arg(kMaxAttempts));
        return false;
    }

    m_failedAttempts.remove(userId);
    for (const User& user : m_userList) {
        if (user.id == userId) {
            m_currentUser = user;
            break;
        }
    }
    emit sessionChanged();
    return true;
}

void AuthController::logout()
{
    if (!authenticated())
        return;
    m_currentUser = User{};
    emit sessionChanged();
}

} // namespace nursera
