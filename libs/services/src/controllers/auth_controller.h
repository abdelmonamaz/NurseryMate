#pragma once

#include "repositories/iuser_repository.h"

#include <QHash>
#include <QObject>
#include <QVariantList>

namespace nursera {

// Authentification par PIN (M01) : sélection d'utilisateur + PIN 4-6
// chiffres (F01-01), verrouillage après 5 échecs (F01-05), création du
// compte Gérant au premier lancement (RG-01.a).
class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY sessionChanged)
    Q_PROPERTY(bool needsSetup READ needsSetup NOTIFY usersChanged)
    Q_PROPERTY(QVariantList users READ users NOTIFY usersChanged)
    Q_PROPERTY(QString currentUserName READ currentUserName NOTIFY sessionChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY sessionChanged)

public:
    static constexpr int kMaxAttempts = 5;

    explicit AuthController(IUserRepository& users, QObject* parent = nullptr);

    bool authenticated() const { return m_currentUser.id > 0; }
    bool needsSetup() const { return m_needsSetup; }
    QVariantList users() const;
    QString currentUserName() const { return m_currentUser.displayName; }
    QString currentRole() const; // "manager" | "seller" | "worker" | ""
    int currentUserId() const { return m_currentUser.id; }

    Q_INVOKABLE void refresh();

    // Premier lancement : crée le compte Gérant et ouvre la session.
    Q_INVOKABLE bool setupManager(const QString& displayName,
                                  const QString& pin,
                                  const QString& confirm);

    Q_INVOKABLE bool login(int userId, const QString& pin);
    Q_INVOKABLE void logout();

signals:
    void sessionChanged();
    void usersChanged();
    void loginFailed(const QString& message);
    void errorOccurred(const QString& message);

private:
    static bool isValidPin(const QString& pin);

    IUserRepository& m_users;
    QList<User> m_userList;
    User m_currentUser;
    bool m_needsSetup = false;
    QHash<int, int> m_failedAttempts; // verrouillage en mémoire (F01-05 minimal)
};

} // namespace nursera
