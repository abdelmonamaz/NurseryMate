#include "repositories/sqlite/sqlite_location_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

QString kindToString(LocationKind kind)
{
    switch (kind) {
    case LocationKind::Field: return QStringLiteral("field");
    case LocationKind::SalesArea: return QStringLiteral("sales_area");
    case LocationKind::Warehouse: return QStringLiteral("warehouse");
    case LocationKind::Greenhouse: break;
    }
    return QStringLiteral("greenhouse");
}

LocationKind kindFromString(const QString& text)
{
    if (text == QLatin1String("field")) return LocationKind::Field;
    if (text == QLatin1String("sales_area")) return LocationKind::SalesArea;
    if (text == QLatin1String("warehouse")) return LocationKind::Warehouse;
    return LocationKind::Greenhouse;
}

} // namespace

SqliteLocationRepository::SqliteLocationRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<QList<Location>> SqliteLocationRepository::all(bool includeInactive)
{
    QString sql = QStringLiteral(
        "SELECT id, name_fr, name_ar, kind, active FROM locations ");
    if (!includeInactive)
        sql += QStringLiteral("WHERE active = 1 ");
    sql += QStringLiteral("ORDER BY id");

    QSqlQuery query(db());
    if (!query.exec(sql))
        return Result<QList<Location>>::fail(QStringLiteral("location.all"),
                                             query.lastError().text());

    QList<Location> locations;
    while (query.next()) {
        Location location;
        location.id = query.value(0).toInt();
        location.nameFr = query.value(1).toString();
        location.nameAr = query.value(2).toString();
        location.kind = kindFromString(query.value(3).toString());
        location.active = query.value(4).toBool();
        locations.append(location);
    }
    return Result<QList<Location>>::ok(std::move(locations));
}

Result<int> SqliteLocationRepository::insert(const Location& location)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO locations (name_fr, name_ar, kind, active) "
        "VALUES (:fr, :ar, :kind, :active)"));
    query.bindValue(QStringLiteral(":fr"), location.nameFr);
    query.bindValue(QStringLiteral(":ar"), location.nameAr);
    query.bindValue(QStringLiteral(":kind"), kindToString(location.kind));
    query.bindValue(QStringLiteral(":active"), location.active ? 1 : 0);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("location.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<bool> SqliteLocationRepository::isReferenced(int locationId)
{
    // Tout passe par les mouvements (ventes, réceptions, ajustements…) ;
    // inventaires et lots référencent aussi directement l'emplacement.
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM stock_moves "
        "              WHERE from_location_id = :l1 OR to_location_id = :l2) "
        "OR EXISTS(SELECT 1 FROM inventories WHERE location_id = :l3) "
        "OR EXISTS(SELECT 1 FROM batches WHERE location_id = :l4)"));
    query.bindValue(QStringLiteral(":l1"), locationId);
    query.bindValue(QStringLiteral(":l2"), locationId);
    query.bindValue(QStringLiteral(":l3"), locationId);
    query.bindValue(QStringLiteral(":l4"), locationId);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("location.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteLocationRepository::remove(int locationId)
{
    // Garde-fou : jamais de suppression d'un référentiel référencé (norme).
    if (const auto referenced = isReferenced(locationId); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("location.referenced"),
            QStringLiteral("Cet emplacement a des mouvements, inventaires "
                           "ou lots — désactivez-le."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM locations WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), locationId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("location.remove"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteLocationRepository::update(const Location& location)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE locations SET name_fr = :fr, name_ar = :ar, kind = :kind, "
        "active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":fr"), location.nameFr);
    query.bindValue(QStringLiteral(":ar"), location.nameAr);
    query.bindValue(QStringLiteral(":kind"), kindToString(location.kind));
    query.bindValue(QStringLiteral(":active"), location.active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), location.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("location.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
