#include "repositories/sqlite/sqlite_settings_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {

SqliteSettingsRepository::SqliteSettingsRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

QString SqliteSettingsRepository::valueOr(const QString& key,
                                          const QString& fallback)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), key);
    if (query.exec() && query.next() && !query.value(0).isNull())
        return query.value(0).toString();
    return fallback;
}

Result<void> SqliteSettingsRepository::setValue(const QString& key,
                                                const QString& value)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO settings (key, value) VALUES (:key, :value) "
        "ON CONFLICT (key) DO UPDATE SET value = excluded.value"));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("settings.set"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<QList<DocCounter>> SqliteSettingsRepository::documentCounters(int year)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT kind, next_number FROM doc_counters WHERE year = :year"));
    query.bindValue(QStringLiteral(":year"), year);
    if (!query.exec())
        return Result<QList<DocCounter>>::fail(QStringLiteral("settings.counters"),
                                               query.lastError().text());
    QList<DocCounter> counters;
    while (query.next()) {
        DocCounter counter;
        counter.kind = query.value(0).toString();
        counter.nextNumber = query.value(1).toInt();
        counters.append(counter);
    }
    return Result<QList<DocCounter>>::ok(std::move(counters));
}

} // namespace nursera
