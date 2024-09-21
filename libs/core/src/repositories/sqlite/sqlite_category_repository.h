#pragma once

#include "repositories/icategory_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteCategoryRepository : public ICategoryRepository
{
public:
    explicit SqliteCategoryRepository(QString connectionName);

    Result<QList<Category>> all(bool includeInactive = false) override;
    Result<int> insert(const Category& category) override;
    Result<void> update(const Category& category) override;
    Result<bool> isReferenced(int categoryId) override;
    Result<void> remove(int categoryId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
