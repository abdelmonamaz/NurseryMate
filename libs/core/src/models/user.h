#pragma once

#include <QString>

namespace nursera {

// Rôles prédéfinis (F01-02) : Gérant / Vendeur / Ouvrier.
enum class UserRole { Manager, Seller, Worker };

struct User
{
    int id = 0;
    QString username;
    QString displayName;
    UserRole role = UserRole::Worker;
    QString lang = QStringLiteral("fr");
    bool active = true;
};

} // namespace nursera
