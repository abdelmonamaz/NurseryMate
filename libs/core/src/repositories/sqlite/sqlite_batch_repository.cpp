#include "repositories/sqlite/sqlite_batch_repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

QString originToString(BatchOrigin origin)
{
    switch (origin) {
    case BatchOrigin::Cutting: return QStringLiteral("cutting");
    case BatchOrigin::Division: return QStringLiteral("division");
    case BatchOrigin::YoungPlant: return QStringLiteral("young_plant");
    case BatchOrigin::Seed: break;
    }
    return QStringLiteral("seed");
}

BatchOrigin originFromString(const QString& text)
{
    if (text == QLatin1String("cutting")) return BatchOrigin::Cutting;
    if (text == QLatin1String("division")) return BatchOrigin::Division;
    if (text == QLatin1String("young_plant")) return BatchOrigin::YoungPlant;
    return BatchOrigin::Seed;
}

} // namespace

SqliteBatchRepository::SqliteBatchRepository(QString connectionName,
                                             IStockRepository& stock)
    : m_connectionName(std::move(connectionName))
    , m_stock(stock)
{
}

Result<Batch> SqliteBatchRepository::create(const BatchDraft& draft)
{
    if (draft.productId <= 0 || draft.qtyInitial <= 0)
        return Result<Batch>::fail(
            QStringLiteral("batch.invalid"),
            QStringLiteral("Produit et quantité initiale obligatoires."));

    const int year = QDate::currentDate().year();
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<Batch>::fail(QStringLiteral("batch.tx"),
                                   database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<Batch>::fail(std::move(code), std::move(message));
    };

    // Numéro L-AAAA-NNN sans trou
    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) VALUES ('batch', :y, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    counter.bindValue(QStringLiteral(":y"), year);
    if (!counter.exec())
        return fail(QStringLiteral("batch.counter"), counter.lastError().text());
    QSqlQuery readCounter(database);
    readCounter.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters WHERE kind = 'batch' AND year = :y"));
    readCounter.bindValue(QStringLiteral(":y"), year);
    if (!readCounter.exec() || !readCounter.next())
        return fail(QStringLiteral("batch.counter"), readCounter.lastError().text());

    Batch batch;
    batch.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    batch.number = QStringLiteral("L-%1-%2")
                       .arg(year)
                       .arg(readCounter.value(0).toInt(), 3, 10, QLatin1Char('0'));

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO batches (uuid, number, product_id, origin, qty_initial, "
        "qty_remaining, location_id, notes, user_id) "
        "VALUES (:uuid, :number, :product, :origin, :qty, :qty, :location, "
        ":notes, :user)"));
    insert.bindValue(QStringLiteral(":uuid"), batch.uuid);
    insert.bindValue(QStringLiteral(":number"), batch.number);
    insert.bindValue(QStringLiteral(":product"), draft.productId);
    insert.bindValue(QStringLiteral(":origin"), originToString(draft.origin));
    insert.bindValue(QStringLiteral(":qty"), draft.qtyInitial);
    insert.bindValue(QStringLiteral(":location"),
                     draft.locationId > 0 ? QVariant(draft.locationId) : QVariant());
    insert.bindValue(QStringLiteral(":notes"),
                     draft.notes.isEmpty() ? QVariant() : QVariant(draft.notes));
    insert.bindValue(QStringLiteral(":user"),
                     draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    if (!insert.exec())
        return fail(QStringLiteral("batch.insert"), insert.lastError().text());
    batch.id = insert.lastInsertId().toInt();

    if (!database.commit()) {
        database.rollback();
        return Result<Batch>::fail(QStringLiteral("batch.commit"),
                                   database.lastError().text());
    }
    return Result<Batch>::ok(std::move(batch));
}

Result<QList<BatchRow>> SqliteBatchRepository::list(bool includeClosed)
{
    QString sql = QStringLiteral(
        "SELECT b.id, b.number, p.name_fr, b.origin, b.qty_initial, "
        "b.qty_remaining, COALESCE(l.name_fr, ''), b.status, b.started_at, "
        "b.product_id, "
        "COALESCE((SELECT SUM(qty) FROM batch_events e "
        "          WHERE e.batch_id = b.id AND e.kind = 'loss'), 0), "
        "COALESCE((SELECT SUM(qty) FROM batch_events e "
        "          WHERE e.batch_id = b.id AND e.kind = 'sellable'), 0) "
        "FROM batches b "
        "JOIN products p ON p.id = b.product_id "
        "LEFT JOIN locations l ON l.id = b.location_id ");
    if (!includeClosed)
        sql += QStringLiteral("WHERE b.status = 'growing' ");
    sql += QStringLiteral("ORDER BY b.id DESC");

    QSqlQuery query(db());
    if (!query.exec(sql))
        return Result<QList<BatchRow>>::fail(QStringLiteral("batch.list"),
                                             query.lastError().text());

    QList<BatchRow> rows;
    while (query.next()) {
        BatchRow row;
        row.id = query.value(0).toInt();
        row.number = query.value(1).toString();
        row.productFr = query.value(2).toString();
        row.origin = originFromString(query.value(3).toString());
        row.qtyInitial = query.value(4).toInt();
        row.qtyRemaining = query.value(5).toInt();
        row.locationFr = query.value(6).toString();
        row.status = query.value(7).toString() == QLatin1String("closed")
            ? BatchStatus::Closed : BatchStatus::Growing;
        row.startedAt = query.value(8).toString();
        row.productId = query.value(9).toInt();
        row.qtyLost = query.value(10).toInt();
        row.qtySellable = query.value(11).toInt();
        rows.append(row);
    }
    return Result<QList<BatchRow>>::ok(std::move(rows));
}

Result<void> SqliteBatchRepository::consumeRemaining(QSqlDatabase& database,
                                                     int batchId, int qty)
{
    QSqlQuery current(database);
    current.prepare(QStringLiteral(
        "SELECT qty_remaining, status FROM batches WHERE id = :id"));
    current.bindValue(QStringLiteral(":id"), batchId);
    if (!current.exec() || !current.next())
        return Result<void>::fail(QStringLiteral("batch.notFound"),
                                  QStringLiteral("Lot introuvable."));
    if (current.value(1).toString() != QLatin1String("growing"))
        return Result<void>::fail(QStringLiteral("batch.closed"),
                                  QStringLiteral("Lot clôturé."));
    const int remaining = current.value(0).toInt();
    if (qty <= 0 || qty > remaining)
        return Result<void>::fail(
            QStringLiteral("batch.qty"),
            QStringLiteral("Quantité invalide (restant : %1).").arg(remaining));

    const int newRemaining = remaining - qty;
    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE batches SET qty_remaining = :rem, "
        "status = CASE WHEN :rem2 = 0 THEN 'closed' ELSE 'growing' END "
        "WHERE id = :id"));
    update.bindValue(QStringLiteral(":rem"), newRemaining);
    update.bindValue(QStringLiteral(":rem2"), newRemaining);
    update.bindValue(QStringLiteral(":id"), batchId);
    if (!update.exec())
        return Result<void>::fail(QStringLiteral("batch.update"),
                                  update.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteBatchRepository::recordLoss(int batchId, int qty,
                                               const QString& lossReason,
                                               const QString& note, int userId)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("batch.tx"),
                                  database.lastError().text());

    if (auto consumed = consumeRemaining(database, batchId, qty); !consumed) {
        database.rollback();
        return consumed;
    }

    QSqlQuery event(database);
    event.prepare(QStringLiteral(
        "INSERT INTO batch_events (uuid, batch_id, kind, qty, loss_reason, "
        "note, user_id) VALUES (:uuid, :batch, 'loss', :qty, :reason, :note, :user)"));
    event.bindValue(QStringLiteral(":uuid"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
    event.bindValue(QStringLiteral(":batch"), batchId);
    event.bindValue(QStringLiteral(":qty"), qty);
    event.bindValue(QStringLiteral(":reason"),
                    lossReason.isEmpty() ? QStringLiteral("other") : lossReason);
    event.bindValue(QStringLiteral(":note"),
                    note.isEmpty() ? QVariant() : QVariant(note));
    event.bindValue(QStringLiteral(":user"),
                    userId > 0 ? QVariant(userId) : QVariant());
    if (!event.exec()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("batch.lossEvent"),
                                  event.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("batch.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<void> SqliteBatchRepository::recordSellable(int batchId, int qty,
                                                   int variantId, int locationId,
                                                   int userId)
{
    if (variantId <= 0 || locationId <= 0)
        return Result<void>::fail(
            QStringLiteral("batch.sellable"),
            QStringLiteral("Conditionnement et emplacement obligatoires."));

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("batch.tx"),
                                  database.lastError().text());
    const auto fail = [&database](Error error) {
        database.rollback();
        return Result<void>::fail(std::move(error));
    };

    if (auto consumed = consumeRemaining(database, batchId, qty); !consumed)
        return fail(consumed.error());

    QSqlQuery event(database);
    event.prepare(QStringLiteral(
        "INSERT INTO batch_events (uuid, batch_id, kind, qty, variant_id, "
        "to_location_id, user_id) "
        "VALUES (:uuid, :batch, 'sellable', :qty, :variant, :location, :user)"));
    event.bindValue(QStringLiteral(":uuid"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
    event.bindValue(QStringLiteral(":batch"), batchId);
    event.bindValue(QStringLiteral(":qty"), qty);
    event.bindValue(QStringLiteral(":variant"), variantId);
    event.bindValue(QStringLiteral(":location"), locationId);
    event.bindValue(QStringLiteral(":user"),
                    userId > 0 ? QVariant(userId) : QVariant());
    if (!event.exec())
        return fail(Error{QStringLiteral("batch.sellableEvent"),
                          event.lastError().text()});

    // Entrée de stock commercial (RG-08.a)
    StockMove entry;
    entry.kind = MoveKind::In;
    entry.variantId = variantId;
    entry.toLocationId = locationId;
    entry.qty = qty;
    entry.refKind = QStringLiteral("batch");
    entry.refId = batchId;
    entry.userId = userId;
    if (auto moved = m_stock.recordMove(entry, /*ownTransaction=*/false); !moved)
        return fail(moved.error());

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("batch.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<QList<BatchEventRow>> SqliteBatchRepository::events(int batchId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT e.id, e.kind, e.qty, COALESCE(e.loss_reason, ''), "
        "       COALESCE(v.packaging, ''), COALESCE(l.name_fr, ''), "
        "       COALESCE(e.note, ''), e.created_at, "
        "       COALESCE(u.display_name, '') "
        "FROM batch_events e "
        "LEFT JOIN variants v ON v.id = e.variant_id "
        "LEFT JOIN locations l ON l.id = e.to_location_id "
        "LEFT JOIN users u ON u.id = e.user_id "
        "WHERE e.batch_id = :id ORDER BY e.id DESC"));
    query.bindValue(QStringLiteral(":id"), batchId);
    if (!query.exec())
        return Result<QList<BatchEventRow>>::fail(QStringLiteral("batch.events"),
                                                  query.lastError().text());
    QList<BatchEventRow> rows;
    while (query.next()) {
        BatchEventRow row;
        row.id = query.value(0).toInt();
        row.kind = query.value(1).toString();
        row.qty = query.value(2).toInt();
        row.lossReason = query.value(3).toString();
        row.variantLabel = query.value(4).toString();
        row.locationFr = query.value(5).toString();
        row.note = query.value(6).toString();
        row.createdAt = query.value(7).toString();
        row.userName = query.value(8).toString();
        rows.append(row);
    }
    return Result<QList<BatchEventRow>>::ok(std::move(rows));
}

Result<void> SqliteBatchRepository::cancelLastEvent(int batchId, int userId)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("batch.tx"),
                                  database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<void>::fail(std::move(code), std::move(message));
    };

    QSqlQuery last(database);
    last.prepare(QStringLiteral(
        "SELECT id, kind, qty, variant_id, to_location_id, loss_reason "
        "FROM batch_events WHERE batch_id = :id ORDER BY id DESC LIMIT 1"));
    last.bindValue(QStringLiteral(":id"), batchId);
    if (!last.exec())
        return fail(QStringLiteral("batch.cancel"), last.lastError().text());
    if (!last.next())
        return fail(QStringLiteral("batch.noEvent"),
                    QStringLiteral("Aucun événement à annuler sur ce lot."));
    const int qty = last.value(2).toInt();
    if (qty <= 0)
        return fail(QStringLiteral("batch.alreadyCancelled"),
                    QStringLiteral("Le dernier événement est déjà une "
                                   "correction — ressaisissez le bon."));
    const QString kind = last.value(1).toString();
    const int variantId = last.value(3).toInt();
    const int locationId = last.value(4).toInt();

    // Restant restauré ; le lot rouvre s'il s'était auto-clôturé à 0.
    QSqlQuery restore(database);
    restore.prepare(QStringLiteral(
        "UPDATE batches SET qty_remaining = qty_remaining + :qty, "
        "status = 'growing' WHERE id = :id"));
    restore.bindValue(QStringLiteral(":qty"), qty);
    restore.bindValue(QStringLiteral(":id"), batchId);
    if (!restore.exec())
        return fail(QStringLiteral("batch.cancel"), restore.lastError().text());

    // Contre-événement (qty négative) : la trace reste, les stats se
    // neutralisent (pertes nettes, rapports par motif).
    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "INSERT INTO batch_events (uuid, batch_id, kind, qty, variant_id, "
        "to_location_id, loss_reason, note, user_id) "
        "VALUES (:uuid, :batch, :kind, :qty, :variant, :location, :reason, "
        "'annulation de saisie', :user)"));
    counter.bindValue(QStringLiteral(":uuid"),
                      QUuid::createUuid().toString(QUuid::WithoutBraces));
    counter.bindValue(QStringLiteral(":batch"), batchId);
    counter.bindValue(QStringLiteral(":kind"), kind);
    counter.bindValue(QStringLiteral(":qty"), -qty);
    counter.bindValue(QStringLiteral(":variant"),
                      variantId > 0 ? QVariant(variantId) : QVariant());
    counter.bindValue(QStringLiteral(":location"),
                      locationId > 0 ? QVariant(locationId) : QVariant());
    counter.bindValue(QStringLiteral(":reason"),
                      last.value(5).isNull() ? QVariant() : last.value(5));
    counter.bindValue(QStringLiteral(":user"),
                      userId > 0 ? QVariant(userId) : QVariant());
    if (!counter.exec())
        return fail(QStringLiteral("batch.cancel"), counter.lastError().text());

    // Un passage en vendable annulé ressort du stock commercial.
    if (kind == QLatin1String("sellable")) {
        StockMove out;
        out.kind = MoveKind::Out;
        out.variantId = variantId;
        out.fromLocationId = locationId;
        out.qty = qty;
        out.refKind = QStringLiteral("batch");
        out.refId = batchId;
        out.reason = QStringLiteral("annulation passage en vendable");
        out.userId = userId;
        if (auto moved = m_stock.recordMove(out, /*ownTransaction=*/false);
            !moved)
            return fail(moved.error().code, moved.error().message);
    }

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("batch.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<void> SqliteBatchRepository::recordTreatment(int batchId,
                                                   const QString& kind,
                                                   const QString& productUsed,
                                                   const QString& dose,
                                                   const QString& note,
                                                   int userId)
{
    static const QStringList kKinds = {
        QStringLiteral("watering"), QStringLiteral("fertilization"),
        QStringLiteral("phyto"), QStringLiteral("pruning"),
        QStringLiteral("other"),
    };
    if (!kKinds.contains(kind))
        return Result<void>::fail(QStringLiteral("treatment.kind"),
                                  QStringLiteral("Type d'intervention inconnu."));
    // Un traitement phyto sans produit n'est pas exploitable (traçabilité).
    if (kind == QLatin1String("phyto") && productUsed.trimmed().isEmpty())
        return Result<void>::fail(
            QStringLiteral("treatment.product"),
            QStringLiteral("Le produit utilisé est obligatoire pour un "
                           "traitement phytosanitaire."));

    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO batch_treatments (uuid, batch_id, kind, product_used, "
        "dose, note, user_id) "
        "VALUES (:uuid, :batch, :kind, :product, :dose, :note, :user)"));
    insert.bindValue(QStringLiteral(":uuid"),
                     QUuid::createUuid().toString(QUuid::WithoutBraces));
    insert.bindValue(QStringLiteral(":batch"), batchId);
    insert.bindValue(QStringLiteral(":kind"), kind);
    insert.bindValue(QStringLiteral(":product"),
                     productUsed.trimmed().isEmpty()
                         ? QVariant() : QVariant(productUsed.trimmed()));
    insert.bindValue(QStringLiteral(":dose"),
                     dose.trimmed().isEmpty() ? QVariant()
                                              : QVariant(dose.trimmed()));
    insert.bindValue(QStringLiteral(":note"),
                     note.trimmed().isEmpty() ? QVariant()
                                              : QVariant(note.trimmed()));
    insert.bindValue(QStringLiteral(":user"),
                     userId > 0 ? QVariant(userId) : QVariant());
    if (!insert.exec())
        return Result<void>::fail(QStringLiteral("treatment.insert"),
                                  insert.lastError().text());
    return Result<void>::ok();
}

Result<QList<BatchTreatmentRow>> SqliteBatchRepository::treatments(int batchId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT t.id, t.kind, COALESCE(t.product_used, ''), "
        "       COALESCE(t.dose, ''), COALESCE(t.note, ''), t.created_at, "
        "       COALESCE(u.display_name, '') "
        "FROM batch_treatments t LEFT JOIN users u ON u.id = t.user_id "
        "WHERE t.batch_id = :id ORDER BY t.id DESC"));
    query.bindValue(QStringLiteral(":id"), batchId);
    if (!query.exec())
        return Result<QList<BatchTreatmentRow>>::fail(
            QStringLiteral("treatment.list"), query.lastError().text());
    QList<BatchTreatmentRow> rows;
    while (query.next()) {
        BatchTreatmentRow row;
        row.id = query.value(0).toInt();
        row.kind = query.value(1).toString();
        row.productUsed = query.value(2).toString();
        row.dose = query.value(3).toString();
        row.note = query.value(4).toString();
        row.createdAt = query.value(5).toString();
        row.userName = query.value(6).toString();
        rows.append(row);
    }
    return Result<QList<BatchTreatmentRow>>::ok(std::move(rows));
}

Result<bool> SqliteBatchRepository::isReferenced(int batchId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM batch_events WHERE batch_id = :e) "
        "OR EXISTS(SELECT 1 FROM batch_treatments WHERE batch_id = :t)"));
    query.bindValue(QStringLiteral(":e"), batchId);
    query.bindValue(QStringLiteral(":t"), batchId);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("batch.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteBatchRepository::remove(int batchId)
{
    // Garde-fou : un lot avec des événements se corrige, ne se supprime pas.
    if (const auto referenced = isReferenced(batchId); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("batch.referenced"),
            QStringLiteral("Ce lot a des événements — corrigez-les par "
                           "contre-passation."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM batches WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), batchId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("batch.remove"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
