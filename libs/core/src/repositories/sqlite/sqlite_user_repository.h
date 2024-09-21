#pragma once

#include "repositories/iuser_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteUserRepository : public IUserRepository
{
public:
    explicit SqliteUserRepository(QString connectionName);

    Result<QList<User>> activeUsers() override;
    Result<QList<User>> allUsers() override;
    Result<int> totalCount() override;
    Result<int> insert(const User& user, const QString& pinHash) override;
    Result<QString> pinHashOf(int userId) override;
    Result<void> setPin(int userId, const QString& pinHash) override;
    Result<void> update(const User& user) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
