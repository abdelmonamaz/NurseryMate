#pragma once

#include "common/result.h"
#include "models/user.h"

#include <QList>

namespace nursera {

class IUserRepository
{
public:
    virtual ~IUserRepository() = default;

    virtual Result<QList<User>> activeUsers() = 0;
    // Tous les comptes, y compris désactivés (administration — réactivation).
    virtual Result<QList<User>> allUsers() = 0;
    // Nombre total de comptes, actifs ou non (détection du 1er lancement).
    virtual Result<int> totalCount() = 0;
    virtual Result<int> insert(const User& user, const QString& pinHash) = 0;
    virtual Result<QString> pinHashOf(int userId) = 0;
    virtual Result<void> setPin(int userId, const QString& pinHash) = 0;
    virtual Result<void> update(const User& user) = 0;
};

} // namespace nursera
