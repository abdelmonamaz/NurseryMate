#include "repositories/sqlite/sqlite_user_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

QString roleToString(UserRole role)
{
    switch (role) {
    case UserRole::Manager: return QStringLiteral("manager");
    case UserRole::Seller: return QStringLiteral("seller");
    case UserRole::Worker: break;
    }
    return QStringLiteral("worker");
}

UserRole roleFromString(const QString& text)
{
    if (text == QLatin1String("manager")) return UserRole::Manager;
    if (text == QLatin1String("seller")) return UserRole::Seller;
    return UserRole::Worker;
}

} // namespace

SqliteUserRepository::SqliteUserRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

namespace {

Result<QList<User>> listUsers(QSqlDatabase database, bool activeOnly)
{
    QSqlQuery query(database);
    QString sql = QStringLiteral(
        "SELECT id, username, display_name, role, lang, active FROM users ");
    if (activeOnly)
        sql += QStringLiteral("WHERE active = 1 ");
    sql += QStringLiteral("ORDER BY id");
    if (!query.exec(sql))
        return Result<QList<User>>::fail(QStringLiteral("user.all"),
                                         query.lastError().text());

    QList<User> users;
    while (query.next()) {
        User user;
        user.id = query.value(0).toInt();
        user.username = query.value(1).toString();
        user.displayName = query.value(2).toString();
        user.role = roleFromString(query.value(3).toString());
        user.lang = query.value(4).toString();
        user.active = query.value(5).toBool();
        users.append(user);
    }
    return Result<QList<User>>::ok(std::move(users));
}

} // namespace

Result<QList<User>> SqliteUserRepository::activeUsers()
{
    return listUsers(db(), /*activeOnly=*/true);
}

Result<QList<User>> SqliteUserRepository::allUsers()
{
    return listUsers(db(), /*activeOnly=*/false);
}

Result<int> SqliteUserRepository::totalCount()
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("SELECT count(*) FROM users")) || !query.next())
        return Result<int>::fail(QStringLiteral("user.count"),
                                 query.lastError().text());
    return Result<int>::ok(query.value(0).toInt());
}

Result<int> SqliteUserRepository::insert(const User& user, const QString& pinHash)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO users (username, display_name, role, pin_hash, lang, active) "
        "VALUES (:username, :name, :role, :pin, :lang, :active)"));
    query.bindValue(QStringLiteral(":username"), user.username);
    query.bindValue(QStringLiteral(":name"), user.displayName);
    query.bindValue(QStringLiteral(":role"), roleToString(user.role));
    query.bindValue(QStringLiteral(":pin"), pinHash);
    query.bindValue(QStringLiteral(":lang"), user.lang);
    query.bindValue(QStringLiteral(":active"), user.active ? 1 : 0);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("user.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<QString> SqliteUserRepository::pinHashOf(int userId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT pin_hash FROM users WHERE id = :id AND active = 1"));
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec())
        return Result<QString>::fail(QStringLiteral("user.pin"),
                                     query.lastError().text());
    if (!query.next())
        return Result<QString>::fail(QStringLiteral("user.notFound"),
                                     QStringLiteral("Utilisateur introuvable."));
    return Result<QString>::ok(query.value(0).toString());
}

Result<void> SqliteUserRepository::setPin(int userId, const QString& pinHash)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE users SET pin_hash = :pin WHERE id = :id"));
    query.bindValue(QStringLiteral(":pin"), pinHash);
    query.bindValue(QStringLiteral(":id"), userId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("user.setPin"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteUserRepository::update(const User& user)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE users SET username = :username, display_name = :name, "
        "role = :role, lang = :lang, active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":username"), user.username);
    query.bindValue(QStringLiteral(":name"), user.displayName);
    query.bindValue(QStringLiteral(":role"), roleToString(user.role));
    query.bindValue(QStringLiteral(":lang"), user.lang);
    query.bindValue(QStringLiteral(":active"), user.active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), user.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("user.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
