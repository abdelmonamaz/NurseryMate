#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

QString kindToString(MoveKind kind)
{
    switch (kind) {
    case MoveKind::Out: return QStringLiteral("out");
    case MoveKind::Transfer: return QStringLiteral("transfer");
    case MoveKind::Adjust: return QStringLiteral("adjust");
    case MoveKind::In: break;
    }
    return QStringLiteral("in");
}

MoveKind kindFromString(const QString& text)
{
    if (text == QLatin1String("out")) return MoveKind::Out;
    if (text == QLatin1String("transfer")) return MoveKind::Transfer;
    if (text == QLatin1String("adjust")) return MoveKind::Adjust;
    return MoveKind::In;
}

// Cohérence kind / emplacements (doc 04 §3.3). Retourne un message
// d'erreur, ou une chaîne vide si le mouvement est valide.
QString validateMove(const StockMove& move)
{
    if (move.variantId <= 0)
        return QStringLiteral("Variante manquante.");
    if (move.qty <= 0)
        return QStringLiteral("La quantité doit être strictement positive.");

    const bool hasFrom = move.fromLocationId > 0;
    const bool hasTo = move.toLocationId > 0;
    switch (move.kind) {
    case MoveKind::In:
        if (!hasTo || hasFrom)
            return QStringLiteral("Une entrée exige un emplacement de destination uniquement.");
        break;
    case MoveKind::Out:
        if (!hasFrom || hasTo)
            return QStringLiteral("Une sortie exige un emplacement d'origine uniquement.");
        break;
    case MoveKind::Transfer:
        if (!hasFrom || !hasTo)
            return QStringLiteral("Un transfert exige une origine et une destination.");
        if (move.fromLocationId == move.toLocationId)
            return QStringLiteral("L'origine et la destination doivent différer.");
        break;
    case MoveKind::Adjust:
        if (hasFrom == hasTo)
            return QStringLiteral("Un ajustement exige exactement un emplacement "
                                  "(destination = écart positif, origine = écart négatif).");
        break;
    }
    return {};
}

} // namespace

SqliteStockRepository::SqliteStockRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<void> SqliteStockRepository::applyDelta(int variantId, int locationId, int delta)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO stock (variant_id, location_id, qty) VALUES (:variant, :location, :delta) "
        "ON CONFLICT (variant_id, location_id) DO UPDATE SET qty = qty + excluded.qty"));
    query.bindValue(QStringLiteral(":variant"), variantId);
    query.bindValue(QStringLiteral(":location"), locationId);
    query.bindValue(QStringLiteral(":delta"), delta);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("stock.projection"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteStockRepository::recordMove(StockMove move, bool ownTransaction)
{
    if (const QString error = validateMove(move); !error.isEmpty())
        return Result<void>::fail(QStringLiteral("stock.invalid"), error);

    if (move.uuid.isEmpty())
        move.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlDatabase database = db();
    if (ownTransaction && !database.transaction())
        return Result<void>::fail(QStringLiteral("stock.tx"),
                                  database.lastError().text());
    // En mode composé, l'appelant gère commit/rollback : ne rien faire ici.
    const auto rollback = [&database, ownTransaction] {
        if (ownTransaction)
            database.rollback();
    };

    // Idempotence par uuid (rejeu de sync) : déjà appliqué -> succès silencieux.
    QSqlQuery existing(database);
    existing.prepare(QStringLiteral("SELECT 1 FROM stock_moves WHERE uuid = :uuid"));
    existing.bindValue(QStringLiteral(":uuid"), move.uuid);
    if (!existing.exec()) {
        rollback();
        return Result<void>::fail(QStringLiteral("stock.idempotence"),
                                  existing.lastError().text());
    }
    if (existing.next()) {
        if (ownTransaction)
            database.commit();
        return Result<void>::ok();
    }

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO stock_moves (uuid, variant_id, kind, from_location_id, "
        "to_location_id, qty, reason, loss_reason, ref_kind, ref_id, note, "
        "user_id, device_id) "
        "VALUES (:uuid, :variant, :kind, :from, :to, :qty, :reason, "
        ":loss_reason, :ref_kind, :ref_id, :note, :user, :device)"));
    insert.bindValue(QStringLiteral(":uuid"), move.uuid);
    insert.bindValue(QStringLiteral(":variant"), move.variantId);
    insert.bindValue(QStringLiteral(":kind"), kindToString(move.kind));
    insert.bindValue(QStringLiteral(":from"),
                     move.fromLocationId > 0 ? QVariant(move.fromLocationId) : QVariant());
    insert.bindValue(QStringLiteral(":to"),
                     move.toLocationId > 0 ? QVariant(move.toLocationId) : QVariant());
    insert.bindValue(QStringLiteral(":qty"), move.qty);
    insert.bindValue(QStringLiteral(":reason"),
                     move.reason.isEmpty() ? QVariant() : QVariant(move.reason));
    insert.bindValue(QStringLiteral(":loss_reason"),
                     move.lossReason.isEmpty() ? QVariant() : QVariant(move.lossReason));
    insert.bindValue(QStringLiteral(":ref_kind"),
                     move.refKind.isEmpty() ? QVariant() : QVariant(move.refKind));
    insert.bindValue(QStringLiteral(":ref_id"),
                     move.refId > 0 ? QVariant(move.refId) : QVariant());
    insert.bindValue(QStringLiteral(":note"),
                     move.note.isEmpty() ? QVariant() : QVariant(move.note));
    insert.bindValue(QStringLiteral(":user"),
                     move.userId > 0 ? QVariant(move.userId) : QVariant());
    insert.bindValue(QStringLiteral(":device"),
                     move.deviceId > 0 ? QVariant(move.deviceId) : QVariant());
    if (!insert.exec()) {
        rollback();
        return Result<void>::fail(QStringLiteral("stock.insert"),
                                  insert.lastError().text());
    }

    // Projection `stock` (doc 04 §3.3) — la vérité reste stock_moves.
    if (move.fromLocationId > 0) {
        if (auto applied = applyDelta(move.variantId, move.fromLocationId, -move.qty);
            !applied) {
            rollback();
            return applied;
        }
    }
    if (move.toLocationId > 0) {
        if (auto applied = applyDelta(move.variantId, move.toLocationId, move.qty);
            !applied) {
            rollback();
            return applied;
        }
    }

    if (ownTransaction && !database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("stock.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<QList<StockLevel>> SqliteStockRepository::levelsOf(int variantId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT s.location_id, l.name_fr, l.name_ar, s.qty "
        "FROM stock s JOIN locations l ON l.id = s.location_id "
        "WHERE s.variant_id = :variant ORDER BY l.id"));
    query.bindValue(QStringLiteral(":variant"), variantId);
    if (!query.exec())
        return Result<QList<StockLevel>>::fail(QStringLiteral("stock.levels"),
                                               query.lastError().text());

    QList<StockLevel> levels;
    while (query.next()) {
        StockLevel level;
        level.locationId = query.value(0).toInt();
        level.locationFr = query.value(1).toString();
        level.locationAr = query.value(2).toString();
        level.qty = query.value(3).toInt();
        levels.append(level);
    }
    return Result<QList<StockLevel>>::ok(std::move(levels));
}

Result<QList<StockOverviewRow>> SqliteStockRepository::overview(const QString& term,
                                                                int locationId)
{
    QString sql = QStringLiteral(
        "SELECT v.id, p.name_fr, p.name_ar, v.packaging, v.sku, v.alert_threshold, "
        "       COALESCE(SUM(s.qty), 0) "
        "FROM variants v "
        "JOIN products p ON p.id = v.product_id "
        "LEFT JOIN stock s ON s.variant_id = v.id ");
    if (locationId > 0)
        sql += QStringLiteral("AND s.location_id = :location ");
    sql += QStringLiteral("WHERE v.active = 1 AND p.active = 1 ");
    const QString trimmedTerm = term.trimmed();
    if (!trimmedTerm.isEmpty())
        sql += QStringLiteral(
            "AND (p.name_fr LIKE :t1 OR p.name_ar LIKE :t2 OR v.sku LIKE :t3) ");
    sql += QStringLiteral("GROUP BY v.id ORDER BY p.name_fr COLLATE NOCASE, v.packaging");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<StockOverviewRow>>::fail(QStringLiteral("stock.overview"),
                                                     query.lastError().text());
    if (locationId > 0)
        query.bindValue(QStringLiteral(":location"), locationId);
    if (!trimmedTerm.isEmpty()) {
        const QString like = QLatin1Char('%') + trimmedTerm + QLatin1Char('%');
        query.bindValue(QStringLiteral(":t1"), like);
        query.bindValue(QStringLiteral(":t2"), like);
        query.bindValue(QStringLiteral(":t3"), like);
    }
    if (!query.exec())
        return Result<QList<StockOverviewRow>>::fail(QStringLiteral("stock.overview"),
                                                     query.lastError().text());

    QList<StockOverviewRow> rows;
    while (query.next()) {
        StockOverviewRow row;
        row.variantId = query.value(0).toInt();
        row.productFr = query.value(1).toString();
        row.productAr = query.value(2).toString();
        row.packaging = query.value(3).toString();
        row.sku = query.value(4).toString();
        row.alertThreshold = query.value(5).isNull() ? -1 : query.value(5).toInt();
        row.qty = query.value(6).toInt();
        rows.append(row);
    }
    return Result<QList<StockOverviewRow>>::ok(std::move(rows));
}

Result<QList<StockMoveRow>> SqliteStockRepository::history(int variantId, int limit)
{
    QString sql = QStringLiteral(
        "SELECT m.id, m.created_at, m.kind, p.name_fr, v.packaging, "
        "       COALESCE(lf.name_fr, ''), COALESCE(lt.name_fr, ''), m.qty, "
        "       COALESCE(m.ref_kind, ''), COALESCE(m.reason, m.note, ''), "
        "       COALESCE(u.display_name, ''), m.variant_id, "
        "       COALESCE(m.from_location_id, 0), COALESCE(m.to_location_id, 0), "
        "       COALESCE(m.loss_reason, '') "
        "FROM stock_moves m "
        "JOIN variants v ON v.id = m.variant_id "
        "JOIN products p ON p.id = v.product_id "
        "LEFT JOIN locations lf ON lf.id = m.from_location_id "
        "LEFT JOIN locations lt ON lt.id = m.to_location_id "
        "LEFT JOIN users u ON u.id = m.user_id ");
    if (variantId > 0)
        sql += QStringLiteral("WHERE m.variant_id = :variant ");
    sql += QStringLiteral("ORDER BY m.id DESC LIMIT :limit");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<StockMoveRow>>::fail(QStringLiteral("stock.history"),
                                                 query.lastError().text());
    if (variantId > 0)
        query.bindValue(QStringLiteral(":variant"), variantId);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<StockMoveRow>>::fail(QStringLiteral("stock.history"),
                                                 query.lastError().text());

    QList<StockMoveRow> rows;
    while (query.next()) {
        StockMoveRow row;
        row.id = query.value(0).toInt();
        row.createdAt = query.value(1).toString();
        row.kind = kindFromString(query.value(2).toString());
        row.productFr = query.value(3).toString();
        row.packaging = query.value(4).toString();
        row.fromFr = query.value(5).toString();
        row.toFr = query.value(6).toString();
        row.qty = query.value(7).toInt();
        row.refKind = query.value(8).toString();
        row.reason = query.value(9).toString();
        row.userName = query.value(10).toString();
        row.variantId = query.value(11).toInt();
        row.fromLocationId = query.value(12).toInt();
        row.toLocationId = query.value(13).toInt();
        row.lossReason = query.value(14).toString();
        rows.append(row);
    }
    return Result<QList<StockMoveRow>>::ok(std::move(rows));
}

Result<QList<VariantPick>> SqliteStockRepository::searchVariants(const QString& term,
                                                                 int limit)
{
    QString sql = QStringLiteral(
        "SELECT v.id, p.name_fr, v.packaging, v.sku, v.price_ttc, v.vat_rate, "
        "       COALESCE(v.price_pro_ttc, 0) "
        "FROM variants v JOIN products p ON p.id = v.product_id "
        "WHERE v.active = 1 AND p.active = 1 ");
    const QString trimmedTerm = term.trimmed();
    if (!trimmedTerm.isEmpty())
        sql += QStringLiteral(
            "AND (p.name_fr LIKE :t1 OR p.name_ar LIKE :t2 OR v.sku LIKE :t3 "
            "OR v.barcode LIKE :t4) ");
    sql += QStringLiteral("ORDER BY p.name_fr COLLATE NOCASE, v.packaging LIMIT :limit");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<VariantPick>>::fail(QStringLiteral("stock.variants"),
                                                query.lastError().text());
    if (!trimmedTerm.isEmpty()) {
        const QString like = QLatin1Char('%') + trimmedTerm + QLatin1Char('%');
        query.bindValue(QStringLiteral(":t1"), like);
        query.bindValue(QStringLiteral(":t2"), like);
        query.bindValue(QStringLiteral(":t3"), like);
        query.bindValue(QStringLiteral(":t4"), like);
    }
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<VariantPick>>::fail(QStringLiteral("stock.variants"),
                                                query.lastError().text());

    QList<VariantPick> picks;
    while (query.next()) {
        VariantPick pick;
        pick.variantId = query.value(0).toInt();
        pick.label = QStringLiteral("%1 — %2")
                         .arg(query.value(1).toString(), query.value(2).toString());
        pick.sku = query.value(3).toString();
        pick.priceTtcMillimes = query.value(4).toLongLong();
        pick.vatRatePercent = query.value(5).toInt();
        pick.priceProMillimes = query.value(6).toLongLong();
        picks.append(pick);
    }
    return Result<QList<VariantPick>>::ok(std::move(picks));
}

} // namespace nursera
