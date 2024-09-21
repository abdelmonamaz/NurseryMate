#pragma once

#include "repositories/ilocation_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteLocationRepository : public ILocationRepository
{
public:
    explicit SqliteLocationRepository(QString connectionName);

    Result<QList<Location>> all(bool includeInactive = false) override;
    Result<int> insert(const Location& location) override;
    Result<void> update(const Location& location) override;
    Result<bool> isReferenced(int locationId) override;
    Result<void> remove(int locationId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
