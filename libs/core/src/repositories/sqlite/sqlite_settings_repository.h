#pragma once

#include "repositories/isettings_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteSettingsRepository : public ISettingsRepository
{
public:
    explicit SqliteSettingsRepository(QString connectionName);

    QString valueOr(const QString& key, const QString& fallback = {}) override;
    Result<void> setValue(const QString& key, const QString& value) override;
    Result<QList<DocCounter>> documentCounters(int year) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
