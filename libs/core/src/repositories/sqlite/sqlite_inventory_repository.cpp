#include "repositories/sqlite/sqlite_inventory_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

// Namespace fixe pour les uuid v5 des ajustements d'inventaire :
// uuid(ajustement) = v5(ns, "uuidInventaire:variantId").
// Déterministe -> la re-validation ne double jamais un ajustement
// (même garantie d'idempotence que la sync, doc 02 §5.1).
const QUuid kAdjustNamespace(QStringLiteral("{7f0c2e4a-5b1d-4e8a-9c3f-2a6d8e4b1c05}"));

InventoryStatus statusFromString(const QString& text)
{
    if (text == QLatin1String("validated")) return InventoryStatus::Validated;
    if (text == QLatin1String("cancelled")) return InventoryStatus::Cancelled;
    return InventoryStatus::Draft;
}

Inventory inventoryFromQuery(const QSqlQuery& query)
{
    Inventory inventory;
    inventory.id = query.value(0).toInt();
    inventory.uuid = query.value(1).toString();
    inventory.locationId = query.value(2).toInt();
    inventory.status = statusFromString(query.value(3).toString());
    inventory.startedAt = query.value(4).toString();
    inventory.validatedAt = query.value(5).toString();
    inventory.locationFr = query.value(6).toString();
    inventory.locationAr = query.value(7).toString();
    return inventory;
}

const QString kSelectInventory = QStringLiteral(
    "SELECT i.id, i.uuid, i.location_id, i.status, i.started_at, i.validated_at, "
    "       l.name_fr, l.name_ar "
    "FROM inventories i JOIN locations l ON l.id = i.location_id ");

} // namespace

SqliteInventoryRepository::SqliteInventoryRepository(QString connectionName,
                                                     IStockRepository& stock)
    : m_connectionName(std::move(connectionName))
    , m_stock(stock)
{
}

Result<Inventory> SqliteInventoryRepository::loadByLocation(int locationId)
{
    QSqlQuery query(db());
    query.prepare(kSelectInventory + QStringLiteral(
        "WHERE i.location_id = :location AND i.status = 'draft' "
        "ORDER BY i.id DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":location"), locationId);
    if (!query.exec())
        return Result<Inventory>::fail(QStringLiteral("inventory.load"),
                                       query.lastError().text());
    if (!query.next())
        return Result<Inventory>::fail(QStringLiteral("inventory.none"),
                                       QStringLiteral("Aucun brouillon"));
    return Result<Inventory>::ok(inventoryFromQuery(query));
}

Result<Inventory> SqliteInventoryRepository::resumeAnyDraft()
{
    QSqlQuery query(db());
    if (!query.exec(kSelectInventory
                    + QStringLiteral("WHERE i.status = 'draft' ORDER BY i.id LIMIT 1")))
        return Result<Inventory>::fail(QStringLiteral("inventory.resume"),
                                       query.lastError().text());
    if (!query.next())
        return Result<Inventory>::fail(QStringLiteral("inventory.none"),
                                       QStringLiteral("Aucun brouillon"));
    return Result<Inventory>::ok(inventoryFromQuery(query));
}

Result<Inventory> SqliteInventoryRepository::startOrResume(int locationId)
{
    if (locationId <= 0)
        return Result<Inventory>::fail(QStringLiteral("inventory.location"),
                                       QStringLiteral("Emplacement manquant."));

    // Reprise du brouillon existant (un seul par emplacement)
    if (auto existing = loadByLocation(locationId); existing)
        return existing;

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<Inventory>::fail(QStringLiteral("inventory.tx"),
                                       database.lastError().text());

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO inventories (uuid, location_id) VALUES (:uuid, :location)"));
    insert.bindValue(QStringLiteral(":uuid"),
                     QUuid::createUuid().toString(QUuid::WithoutBraces));
    insert.bindValue(QStringLiteral(":location"), locationId);
    if (!insert.exec()) {
        database.rollback();
        return Result<Inventory>::fail(QStringLiteral("inventory.insert"),
                                       insert.lastError().text());
    }
    const int inventoryId = insert.lastInsertId().toInt();

    // Théorique figé depuis la projection `stock` (RG-03.c)
    QSqlQuery snapshot(database);
    snapshot.prepare(QStringLiteral(
        "INSERT INTO inventory_lines (inventory_id, variant_id, qty_expected) "
        "SELECT :inventory, s.variant_id, s.qty FROM stock s "
        "JOIN variants v ON v.id = s.variant_id AND v.active = 1 "
        "WHERE s.location_id = :location"));
    snapshot.bindValue(QStringLiteral(":inventory"), inventoryId);
    snapshot.bindValue(QStringLiteral(":location"), locationId);
    if (!snapshot.exec()) {
        database.rollback();
        return Result<Inventory>::fail(QStringLiteral("inventory.snapshot"),
                                       snapshot.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<Inventory>::fail(QStringLiteral("inventory.commit"),
                                       database.lastError().text());
    }
    return loadByLocation(locationId);
}

Result<QList<InventoryLine>> SqliteInventoryRepository::linesOf(int inventoryId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT il.variant_id, p.name_fr, p.name_ar, v.packaging, v.sku, "
        "       il.qty_expected, il.qty_counted "
        "FROM inventory_lines il "
        "JOIN variants v ON v.id = il.variant_id "
        "JOIN products p ON p.id = v.product_id "
        "WHERE il.inventory_id = :inventory "
        "ORDER BY p.name_fr COLLATE NOCASE, v.packaging"));
    query.bindValue(QStringLiteral(":inventory"), inventoryId);
    if (!query.exec())
        return Result<QList<InventoryLine>>::fail(QStringLiteral("inventory.lines"),
                                                  query.lastError().text());

    QList<InventoryLine> lines;
    while (query.next()) {
        InventoryLine line;
        line.variantId = query.value(0).toInt();
        line.productFr = query.value(1).toString();
        line.productAr = query.value(2).toString();
        line.packaging = query.value(3).toString();
        line.sku = query.value(4).toString();
        line.qtyExpected = query.value(5).toInt();
        line.qtyCounted = query.value(6).isNull() ? -1 : query.value(6).toInt();
        lines.append(line);
    }
    return Result<QList<InventoryLine>>::ok(std::move(lines));
}

Result<void> SqliteInventoryRepository::setCounted(int inventoryId,
                                                   int variantId, int qty)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE inventory_lines SET qty_counted = :qty "
        "WHERE inventory_id = :inventory AND variant_id = :variant "
        "AND inventory_id IN (SELECT id FROM inventories WHERE status = 'draft')"));
    query.bindValue(QStringLiteral(":qty"), qty >= 0 ? QVariant(qty) : QVariant());
    query.bindValue(QStringLiteral(":inventory"), inventoryId);
    query.bindValue(QStringLiteral(":variant"), variantId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("inventory.count"),
                                  query.lastError().text());
    if (query.numRowsAffected() == 0)
        return Result<void>::fail(
            QStringLiteral("inventory.locked"),
            QStringLiteral("Ligne introuvable ou inventaire déjà validé."));
    return Result<void>::ok();
}

Result<int> SqliteInventoryRepository::validate(int inventoryId)
{
    QSqlQuery query(db());
    query.prepare(kSelectInventory + QStringLiteral("WHERE i.id = :id"));
    query.bindValue(QStringLiteral(":id"), inventoryId);
    if (!query.exec() || !query.next())
        return Result<int>::fail(QStringLiteral("inventory.notFound"),
                                 QStringLiteral("Inventaire introuvable."));
    const Inventory inventory = inventoryFromQuery(query);
    if (inventory.status != InventoryStatus::Draft)
        return Result<int>::fail(QStringLiteral("inventory.locked"),
                                 QStringLiteral("Inventaire déjà validé (RG-03.c)."));

    const auto lines = linesOf(inventoryId);
    if (!lines)
        return Result<int>::fail(lines.error());

    // Lignes comptées avec écart -> ajustements idempotents.
    // En cas d'échec partiel, l'inventaire reste en brouillon et une
    // nouvelle validation reprend sans doubler (uuid v5 déterministes).
    int adjustments = 0;
    for (const InventoryLine& line : lines.value()) {
        if (!line.isCounted() || line.gap() == 0)
            continue;

        StockMove move;
        move.kind = MoveKind::Adjust;
        move.variantId = line.variantId;
        move.qty = qAbs(line.gap());
        if (line.gap() > 0)
            move.toLocationId = inventory.locationId;
        else
            move.fromLocationId = inventory.locationId;
        move.refKind = QStringLiteral("inventory");
        move.refId = inventoryId;
        move.reason = QStringLiteral("inventaire");
        move.uuid = QUuid::createUuidV5(
                        kAdjustNamespace,
                        QStringLiteral("%1:%2").arg(inventory.uuid).arg(line.variantId))
                        .toString(QUuid::WithoutBraces);

        if (const auto recorded = m_stock.recordMove(move); !recorded)
            return Result<int>::fail(recorded.error());
        ++adjustments;
    }

    QSqlQuery lock(db());
    lock.prepare(QStringLiteral(
        "UPDATE inventories SET status = 'validated', "
        "validated_at = datetime('now') WHERE id = :id AND status = 'draft'"));
    lock.bindValue(QStringLiteral(":id"), inventoryId);
    if (!lock.exec())
        return Result<int>::fail(QStringLiteral("inventory.lock"),
                                 lock.lastError().text());
    return Result<int>::ok(adjustments);
}

Result<void> SqliteInventoryRepository::cancel(int inventoryId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE inventories SET status = 'cancelled' "
        "WHERE id = :id AND status = 'draft'"));
    query.bindValue(QStringLiteral(":id"), inventoryId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("inventory.cancel"),
                                  query.lastError().text());
    if (query.numRowsAffected() == 0)
        return Result<void>::fail(QStringLiteral("inventory.locked"),
                                  QStringLiteral("Inventaire déjà validé."));
    return Result<void>::ok();
}

} // namespace nursera
