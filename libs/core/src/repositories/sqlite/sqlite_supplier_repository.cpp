#include "repositories/sqlite/sqlite_supplier_repository.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

void bindSupplier(QSqlQuery& query, const Supplier& supplier)
{
    query.bindValue(QStringLiteral(":name"), supplier.name);
    query.bindValue(QStringLiteral(":phone"),
                    supplier.phone.isEmpty() ? QVariant() : QVariant(supplier.phone));
    query.bindValue(QStringLiteral(":email"),
                    supplier.email.isEmpty() ? QVariant() : QVariant(supplier.email));
    query.bindValue(QStringLiteral(":address"),
                    supplier.address.isEmpty() ? QVariant() : QVariant(supplier.address));
    query.bindValue(QStringLiteral(":tax_id"),
                    supplier.taxId.isEmpty() ? QVariant() : QVariant(supplier.taxId));
    query.bindValue(QStringLiteral(":terms"),
                    supplier.paymentTerms.isEmpty() ? QVariant()
                                                    : QVariant(supplier.paymentTerms));
    query.bindValue(QStringLiteral(":notes"),
                    supplier.notes.isEmpty() ? QVariant() : QVariant(supplier.notes));
    query.bindValue(QStringLiteral(":supplies"),
                    supplier.supplies.isEmpty() ? QVariant()
                                                : QVariant(supplier.supplies));
    query.bindValue(QStringLiteral(":lat"),
                    supplier.latitude != 0 ? QVariant(supplier.latitude) : QVariant());
    query.bindValue(QStringLiteral(":lng"),
                    supplier.longitude != 0 ? QVariant(supplier.longitude)
                                            : QVariant());
    query.bindValue(QStringLiteral(":active"), supplier.active ? 1 : 0);
}

} // namespace

SqliteSupplierRepository::SqliteSupplierRepository(QString connectionName,
                                                   IStockRepository& stock)
    : m_connectionName(std::move(connectionName))
    , m_stock(stock)
{
}

Result<QList<Supplier>> SqliteSupplierRepository::search(const QString& term,
                                                         bool includeInactive)
{
    QString sql = QStringLiteral(
        "SELECT id, name, COALESCE(phone, ''), COALESCE(email, ''), "
        "COALESCE(address, ''), COALESCE(tax_id, ''), "
        "COALESCE(payment_terms, ''), COALESCE(notes, ''), active, "
        "COALESCE(supplies, ''), COALESCE(latitude, 0), COALESCE(longitude, 0), "
        // Dette (F07-05) : réceptions − paiements, jamais stockée.
        "COALESCE((SELECT SUM(total_cost) FROM receipts r "
        "          WHERE r.supplier_id = suppliers.id), 0) "
        "- COALESCE((SELECT SUM(amount) FROM supplier_payments sp "
        "            WHERE sp.supplier_id = suppliers.id), 0) "
        "FROM suppliers ");
    QStringList where;
    const QString trimmedTerm = term.trimmed();
    if (!includeInactive)
        where << QStringLiteral("active = 1");
    if (!trimmedTerm.isEmpty())
        where << QStringLiteral("(name LIKE :t1 OR phone LIKE :t2)");
    if (!where.isEmpty())
        sql += QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")) + QLatin1Char(' ');
    sql += QStringLiteral("ORDER BY name COLLATE NOCASE");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<Supplier>>::fail(QStringLiteral("supplier.search"),
                                             query.lastError().text());
    if (!trimmedTerm.isEmpty()) {
        const QString like = QLatin1Char('%') + trimmedTerm + QLatin1Char('%');
        query.bindValue(QStringLiteral(":t1"), like);
        query.bindValue(QStringLiteral(":t2"), like);
    }
    if (!query.exec())
        return Result<QList<Supplier>>::fail(QStringLiteral("supplier.search"),
                                             query.lastError().text());

    QList<Supplier> suppliers;
    while (query.next()) {
        Supplier supplier;
        supplier.id = query.value(0).toInt();
        supplier.name = query.value(1).toString();
        supplier.phone = query.value(2).toString();
        supplier.email = query.value(3).toString();
        supplier.address = query.value(4).toString();
        supplier.taxId = query.value(5).toString();
        supplier.paymentTerms = query.value(6).toString();
        supplier.notes = query.value(7).toString();
        supplier.active = query.value(8).toBool();
        supplier.supplies = query.value(9).toString();
        supplier.latitude = query.value(10).toDouble();
        supplier.longitude = query.value(11).toDouble();
        supplier.balance = Money::fromMillimes(query.value(12).toLongLong());
        suppliers.append(supplier);
    }
    return Result<QList<Supplier>>::ok(std::move(suppliers));
}

Result<int> SqliteSupplierRepository::insert(const Supplier& supplier)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO suppliers (name, phone, email, address, tax_id, "
        "payment_terms, notes, supplies, latitude, longitude, active) "
        "VALUES (:name, :phone, :email, :address, :tax_id, :terms, :notes, "
        ":supplies, :lat, :lng, :active)"));
    bindSupplier(query, supplier);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("supplier.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<void> SqliteSupplierRepository::update(const Supplier& supplier)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE suppliers SET name = :name, phone = :phone, email = :email, "
        "address = :address, tax_id = :tax_id, payment_terms = :terms, "
        "notes = :notes, supplies = :supplies, latitude = :lat, "
        "longitude = :lng, active = :active WHERE id = :id"));
    bindSupplier(query, supplier);
    query.bindValue(QStringLiteral(":id"), supplier.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("supplier.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteSupplierRepository::setActive(int id, bool active)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE suppliers SET active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":active"), active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("supplier.setActive"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<bool> SqliteSupplierRepository::isReferenced(int id)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM receipts WHERE supplier_id = :s1) "
        "OR EXISTS(SELECT 1 FROM purchase_orders WHERE supplier_id = :s2)"));
    query.bindValue(QStringLiteral(":s1"), id);
    query.bindValue(QStringLiteral(":s2"), id);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("supplier.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteSupplierRepository::remove(int id)
{
    // Garde-fou : jamais de suppression d'une fiche référencée (norme).
    if (const auto referenced = isReferenced(id); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("supplier.referenced"),
            QStringLiteral("Ce fournisseur a des transactions — désactivez-le."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM suppliers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("supplier.remove"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteSupplierRepository::updateAverageCost(int variantId, int qty,
                                                         qint64 unitCost)
{
    // Stock total AVANT la réception (jamais négatif pour la pondération)
    QSqlQuery current(db());
    current.prepare(QStringLiteral(
        "SELECT COALESCE((SELECT SUM(qty) FROM stock WHERE variant_id = :v1), 0), "
        "       (SELECT avg_cost FROM variants WHERE id = :v2)"));
    current.bindValue(QStringLiteral(":v1"), variantId);
    current.bindValue(QStringLiteral(":v2"), variantId);
    if (!current.exec() || !current.next())
        return Result<void>::fail(QStringLiteral("supplier.avgCost"),
                                  current.lastError().text());

    const qint64 previousQty = qMax<qint64>(0, current.value(0).toLongLong());
    const qint64 previousAvg = current.value(1).toLongLong();
    const qint64 newAvg = (previousQty + qty) > 0
        ? (previousQty * previousAvg + qty * unitCost + (previousQty + qty) / 2)
            / (previousQty + qty)
        : unitCost;

    QSqlQuery update(db());
    update.prepare(QStringLiteral(
        "UPDATE variants SET avg_cost = :avg WHERE id = :id"));
    update.bindValue(QStringLiteral(":avg"), newAvg);
    update.bindValue(QStringLiteral(":id"), variantId);
    if (!update.exec())
        return Result<void>::fail(QStringLiteral("supplier.avgCost"),
                                  update.lastError().text());
    return Result<void>::ok();
}

Result<int> SqliteSupplierRepository::recordReceipt(const ReceiptDraft& draft)
{
    if (draft.lines.isEmpty())
        return Result<int>::fail(QStringLiteral("receipt.empty"),
                                 QStringLiteral("Aucune ligne de réception."));
    if (draft.locationId <= 0)
        return Result<int>::fail(QStringLiteral("receipt.location"),
                                 QStringLiteral("Choisissez un emplacement."));
    for (const ReceiptLine& line : draft.lines) {
        if (line.variantId <= 0 || line.qty <= 0 || line.unitCost.isNegative())
            return Result<int>::fail(
                QStringLiteral("receipt.line"),
                QStringLiteral("Ligne invalide (quantité > 0, coût >= 0)."));
    }

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<int>::fail(QStringLiteral("receipt.tx"),
                                 database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<int>::fail(std::move(code), std::move(message));
    };

    QSqlQuery insertReceipt(database);
    insertReceipt.prepare(QStringLiteral(
        "INSERT INTO receipts (uuid, supplier_id, location_id, total_cost, "
        "note, user_id) "
        "VALUES (:uuid, :supplier, :location, :total, :note, :user)"));
    insertReceipt.bindValue(QStringLiteral(":uuid"),
                            QUuid::createUuid().toString(QUuid::WithoutBraces));
    insertReceipt.bindValue(QStringLiteral(":supplier"),
                            draft.supplierId > 0 ? QVariant(draft.supplierId)
                                                 : QVariant());
    insertReceipt.bindValue(QStringLiteral(":location"), draft.locationId);
    insertReceipt.bindValue(QStringLiteral(":total"),
                            draft.totalCost().millimes());
    insertReceipt.bindValue(QStringLiteral(":note"),
                            draft.note.isEmpty() ? QVariant() : QVariant(draft.note));
    insertReceipt.bindValue(QStringLiteral(":user"),
                            draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    if (!insertReceipt.exec())
        return fail(QStringLiteral("receipt.insert"),
                    insertReceipt.lastError().text());
    const int receiptId = insertReceipt.lastInsertId().toInt();

    for (const ReceiptLine& line : draft.lines) {
        QSqlQuery insertLine(database);
        insertLine.prepare(QStringLiteral(
            "INSERT INTO receipt_lines (receipt_id, variant_id, qty, unit_cost) "
            "VALUES (:receipt, :variant, :qty, :cost)"));
        insertLine.bindValue(QStringLiteral(":receipt"), receiptId);
        insertLine.bindValue(QStringLiteral(":variant"), line.variantId);
        insertLine.bindValue(QStringLiteral(":qty"), line.qty);
        insertLine.bindValue(QStringLiteral(":cost"), line.unitCost.millimes());
        if (!insertLine.exec())
            return fail(QStringLiteral("receipt.line"),
                        insertLine.lastError().text());

        // CMP avant l'entrée de stock (pondération sur l'existant, F03-09)
        if (const auto avg = updateAverageCost(line.variantId, line.qty,
                                               line.unitCost.millimes());
            !avg)
            return fail(avg.error().code, avg.error().message);

        StockMove entry;
        entry.kind = MoveKind::In;
        entry.variantId = line.variantId;
        entry.toLocationId = draft.locationId;
        entry.qty = line.qty;
        entry.refKind = QStringLiteral("purchase");
        entry.refId = receiptId;
        entry.userId = draft.userId;
        if (const auto moved = m_stock.recordMove(entry, /*ownTransaction=*/false);
            !moved)
            return fail(moved.error().code, moved.error().message);
    }

    if (!database.commit()) {
        database.rollback();
        return Result<int>::fail(QStringLiteral("receipt.commit"),
                                 database.lastError().text());
    }
    return Result<int>::ok(receiptId);
}

Result<QList<ReceiptRow>> SqliteSupplierRepository::recentReceipts(int limit)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT r.id, r.received_at, COALESCE(s.name, ''), l.name_fr, "
        "       (SELECT count(*) FROM receipt_lines rl WHERE rl.receipt_id = r.id), "
        "       (SELECT COALESCE(SUM(qty), 0) FROM receipt_lines rl "
        "        WHERE rl.receipt_id = r.id), "
        "       r.total_cost "
        "FROM receipts r "
        "LEFT JOIN suppliers s ON s.id = r.supplier_id "
        "JOIN locations l ON l.id = r.location_id "
        "ORDER BY r.id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<ReceiptRow>>::fail(QStringLiteral("receipt.recent"),
                                               query.lastError().text());

    QList<ReceiptRow> rows;
    while (query.next()) {
        ReceiptRow row;
        row.id = query.value(0).toInt();
        row.receivedAt = query.value(1).toString();
        row.supplierName = query.value(2).toString();
        row.locationFr = query.value(3).toString();
        row.lineCount = query.value(4).toInt();
        row.totalQty = query.value(5).toInt();
        row.totalCost = Money::fromMillimes(query.value(6).toLongLong());
        rows.append(row);
    }
    return Result<QList<ReceiptRow>>::ok(std::move(rows));
}

// ── Paiements fournisseurs (F07-05) ───────────────────────────

Result<int> SqliteSupplierRepository::recordSupplierPayment(
    int supplierId, Money amount, const QString& method,
    const QString& chequeNumber, const QString& chequeDue,
    const QString& note, int userId)
{
    if (supplierId <= 0)
        return Result<int>::fail(QStringLiteral("supPay.supplier"),
                                 QStringLiteral("Fournisseur obligatoire."));
    if (amount.millimes() <= 0)
        return Result<int>::fail(QStringLiteral("supPay.amount"),
                                 QStringLiteral("Montant invalide."));
    if (method == QLatin1String("cheque") && chequeDue.trimmed().isEmpty())
        return Result<int>::fail(
            QStringLiteral("supPay.due"),
            QStringLiteral("Indiquez l'échéance du chèque."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO supplier_payments (uuid, supplier_id, amount, method, "
        "cheque_number, cheque_due, note, user_id) "
        "VALUES (:uuid, :supplier, :amount, :method, :cheque, :due, :note, "
        ":user)"));
    query.bindValue(QStringLiteral(":uuid"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":supplier"), supplierId);
    query.bindValue(QStringLiteral(":amount"), amount.millimes());
    query.bindValue(QStringLiteral(":method"), method);
    query.bindValue(QStringLiteral(":cheque"),
                    chequeNumber.isEmpty() ? QVariant() : QVariant(chequeNumber));
    query.bindValue(QStringLiteral(":due"),
                    chequeDue.isEmpty() ? QVariant() : QVariant(chequeDue));
    query.bindValue(QStringLiteral(":note"),
                    note.isEmpty() ? QVariant() : QVariant(note));
    query.bindValue(QStringLiteral(":user"),
                    userId > 0 ? QVariant(userId) : QVariant());
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("supPay.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<QList<SupplierPaymentRow>> SqliteSupplierRepository::paymentsOf(
    int supplierId, int limit)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, date(created_at), amount, method, "
        "COALESCE(cheque_number, ''), COALESCE(cheque_due, ''), "
        "COALESCE(note, '') FROM supplier_payments "
        "WHERE supplier_id = :supplier ORDER BY id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":supplier"), supplierId);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<SupplierPaymentRow>>::fail(
            QStringLiteral("supPay.list"), query.lastError().text());
    QList<SupplierPaymentRow> rows;
    while (query.next()) {
        SupplierPaymentRow row;
        row.id = query.value(0).toInt();
        row.date = query.value(1).toString();
        row.amount = Money::fromMillimes(query.value(2).toLongLong());
        row.method = query.value(3).toString();
        row.chequeNumber = query.value(4).toString();
        row.chequeDue = query.value(5).toString();
        row.note = query.value(6).toString();
        rows.append(row);
    }
    return Result<QList<SupplierPaymentRow>>::ok(std::move(rows));
}

Result<Money> SqliteSupplierRepository::supplierBalance(int supplierId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT COALESCE((SELECT SUM(total_cost) FROM receipts r "
        "                 WHERE r.supplier_id = :s1), 0) "
        "- COALESCE((SELECT SUM(amount) FROM supplier_payments sp "
        "            WHERE sp.supplier_id = :s2), 0)"));
    query.bindValue(QStringLiteral(":s1"), supplierId);
    query.bindValue(QStringLiteral(":s2"), supplierId);
    if (!query.exec() || !query.next())
        return Result<Money>::fail(QStringLiteral("supPay.balance"),
                                   query.lastError().text());
    return Result<Money>::ok(Money::fromMillimes(query.value(0).toLongLong()));
}

Result<QList<DueChequeRow>> SqliteSupplierRepository::dueCheques(int daysAhead)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT s.name, sp.amount, COALESCE(sp.cheque_number, ''), "
        "sp.cheque_due, sp.cheque_due < date('now') "
        "FROM supplier_payments sp JOIN suppliers s ON s.id = sp.supplier_id "
        "WHERE sp.method = 'cheque' AND sp.cheque_due IS NOT NULL "
        "AND sp.cheque_due <= date('now', :ahead) "
        "ORDER BY sp.cheque_due ASC"));
    query.bindValue(QStringLiteral(":ahead"),
                    QStringLiteral("+%1 day").arg(daysAhead));
    if (!query.exec())
        return Result<QList<DueChequeRow>>::fail(QStringLiteral("supPay.due"),
                                                 query.lastError().text());
    QList<DueChequeRow> rows;
    while (query.next()) {
        DueChequeRow row;
        row.supplierName = query.value(0).toString();
        row.amount = Money::fromMillimes(query.value(1).toLongLong());
        row.chequeNumber = query.value(2).toString();
        row.dueDate = query.value(3).toString();
        row.overdue = query.value(4).toBool();
        rows.append(row);
    }
    return Result<QList<DueChequeRow>>::ok(std::move(rows));
}

// ── Catalogue fournisseur (F07-06/07/09/10) ───────────────────

Result<void> SqliteSupplierRepository::linkProduct(int supplierId, int variantId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO supplier_products (supplier_id, variant_id) "
        "VALUES (:supplier, :variant)"));
    query.bindValue(QStringLiteral(":supplier"), supplierId);
    query.bindValue(QStringLiteral(":variant"), variantId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("supplier.link"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteSupplierRepository::unlinkProduct(int supplierId,
                                                     int variantId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "DELETE FROM supplier_products "
        "WHERE supplier_id = :supplier AND variant_id = :variant"));
    query.bindValue(QStringLiteral(":supplier"), supplierId);
    query.bindValue(QStringLiteral(":variant"), variantId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("supplier.unlink"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<QList<SupplierProductRow>> SqliteSupplierRepository::productsOf(
    int supplierId)
{
    // Univers = liens déclarés UNION produits réellement livrés ;
    // les statistiques viennent des réceptions (jamais de saisie en double).
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT v.id, p.name_fr || ' — ' || v.packaging, COALESCE(v.sku, ''), "
        "  EXISTS(SELECT 1 FROM supplier_products sp "
        "         WHERE sp.supplier_id = :s1 AND sp.variant_id = v.id), "
        "  COALESCE(st.qty_supplied, 0), COALESCE(st.delivery_count, 0), "
        "  COALESCE((SELECT rl2.unit_cost FROM receipt_lines rl2 "
        "            JOIN receipts r2 ON r2.id = rl2.receipt_id "
        "            WHERE r2.supplier_id = :s2 AND rl2.variant_id = v.id "
        "            ORDER BY r2.id DESC LIMIT 1), 0), "
        "  COALESCE(st.total_cost, 0), COALESCE(st.last_delivery, '') "
        "FROM variants v JOIN products p ON p.id = v.product_id "
        "LEFT JOIN (SELECT rl.variant_id AS vid, SUM(rl.qty) AS qty_supplied, "
        "           COUNT(DISTINCT r.id) AS delivery_count, "
        "           SUM(rl.qty * rl.unit_cost) AS total_cost, "
        "           MAX(date(r.received_at)) AS last_delivery "
        "           FROM receipt_lines rl "
        "           JOIN receipts r ON r.id = rl.receipt_id "
        "           WHERE r.supplier_id = :s3 GROUP BY rl.variant_id) st "
        "  ON st.vid = v.id "
        "WHERE st.vid IS NOT NULL "
        "   OR EXISTS(SELECT 1 FROM supplier_products sp2 "
        "             WHERE sp2.supplier_id = :s4 AND sp2.variant_id = v.id) "
        "ORDER BY st.last_delivery DESC, p.name_fr COLLATE NOCASE"));
    query.bindValue(QStringLiteral(":s1"), supplierId);
    query.bindValue(QStringLiteral(":s2"), supplierId);
    query.bindValue(QStringLiteral(":s3"), supplierId);
    query.bindValue(QStringLiteral(":s4"), supplierId);
    if (!query.exec())
        return Result<QList<SupplierProductRow>>::fail(
            QStringLiteral("supplier.products"), query.lastError().text());

    QList<SupplierProductRow> rows;
    while (query.next()) {
        SupplierProductRow row;
        row.variantId = query.value(0).toInt();
        row.label = query.value(1).toString();
        row.sku = query.value(2).toString();
        row.linked = query.value(3).toBool();
        row.qtySupplied = query.value(4).toInt();
        row.deliveryCount = query.value(5).toInt();
        row.lastCost = Money::fromMillimes(query.value(6).toLongLong());
        const qint64 totalCost = query.value(7).toLongLong();
        row.avgCost = row.qtySupplied > 0
            ? Money::fromMillimes((totalCost + row.qtySupplied / 2)
                                  / row.qtySupplied)
            : Money{};
        row.lastDelivery = query.value(8).toString();
        rows.append(row);
    }
    return Result<QList<SupplierProductRow>>::ok(std::move(rows));
}

Result<QList<SupplierOfferRow>> SqliteSupplierRepository::suppliersFor(
    int variantId)
{
    // « Meilleure offre » : dernier prix croissant, jamais-livrés en queue.
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT s.id, s.name, "
        "  EXISTS(SELECT 1 FROM supplier_products sp "
        "         WHERE sp.variant_id = :v1 AND sp.supplier_id = s.id), "
        "  COALESCE(st.qty, 0), COALESCE(st.cnt, 0), "
        "  (SELECT rl2.unit_cost FROM receipt_lines rl2 "
        "   JOIN receipts r2 ON r2.id = rl2.receipt_id "
        "   WHERE rl2.variant_id = :v2 AND r2.supplier_id = s.id "
        "   ORDER BY r2.id DESC LIMIT 1) AS last_cost, "
        "  COALESCE(st.total, 0), COALESCE(st.last_d, '') "
        "FROM suppliers s "
        "LEFT JOIN (SELECT r.supplier_id AS sid, SUM(rl.qty) AS qty, "
        "           COUNT(DISTINCT r.id) AS cnt, "
        "           SUM(rl.qty * rl.unit_cost) AS total, "
        "           MAX(date(r.received_at)) AS last_d "
        "           FROM receipt_lines rl "
        "           JOIN receipts r ON r.id = rl.receipt_id "
        "           WHERE rl.variant_id = :v3 AND r.supplier_id IS NOT NULL "
        "           GROUP BY r.supplier_id) st ON st.sid = s.id "
        "WHERE st.sid IS NOT NULL "
        "   OR EXISTS(SELECT 1 FROM supplier_products sp2 "
        "             WHERE sp2.variant_id = :v4 AND sp2.supplier_id = s.id) "
        "ORDER BY (last_cost IS NULL), last_cost ASC"));
    query.bindValue(QStringLiteral(":v1"), variantId);
    query.bindValue(QStringLiteral(":v2"), variantId);
    query.bindValue(QStringLiteral(":v3"), variantId);
    query.bindValue(QStringLiteral(":v4"), variantId);
    if (!query.exec())
        return Result<QList<SupplierOfferRow>>::fail(
            QStringLiteral("supplier.offers"), query.lastError().text());

    QList<SupplierOfferRow> rows;
    while (query.next()) {
        SupplierOfferRow row;
        row.supplierId = query.value(0).toInt();
        row.supplierName = query.value(1).toString();
        row.linked = query.value(2).toBool();
        row.qtySupplied = query.value(3).toInt();
        row.deliveryCount = query.value(4).toInt();
        row.lastCost = Money::fromMillimes(query.value(5).toLongLong());
        const qint64 totalCost = query.value(6).toLongLong();
        row.avgCost = row.qtySupplied > 0
            ? Money::fromMillimes((totalCost + row.qtySupplied / 2)
                                  / row.qtySupplied)
            : Money{};
        row.lastDelivery = query.value(7).toString();
        rows.append(row);
    }
    return Result<QList<SupplierOfferRow>>::ok(std::move(rows));
}

Result<QList<PricePoint>> SqliteSupplierRepository::priceHistoryOf(
    int variantId, int limit)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT date(r.received_at), rl.unit_cost, COALESCE(s.name, '') "
        "FROM receipt_lines rl "
        "JOIN receipts r ON r.id = rl.receipt_id "
        "LEFT JOIN suppliers s ON s.id = r.supplier_id "
        "WHERE rl.variant_id = :variant "
        "ORDER BY r.id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":variant"), variantId);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<PricePoint>>::fail(
            QStringLiteral("supplier.priceHistory"), query.lastError().text());

    QList<PricePoint> points;
    while (query.next()) {
        PricePoint point;
        point.date = query.value(0).toString();
        point.unitCost = Money::fromMillimes(query.value(1).toLongLong());
        point.supplierName = query.value(2).toString();
        points.prepend(point); // ordre chronologique pour la courbe
    }
    return Result<QList<PricePoint>>::ok(std::move(points));
}

// ── Commandes d'achat (F07-02/03) ─────────────────────────────

Result<PurchaseOrder> SqliteSupplierRepository::createOrder(
    const PurchaseOrderDraft& draft)
{
    if (draft.supplierId <= 0)
        return Result<PurchaseOrder>::fail(QStringLiteral("po.supplier"),
                                           QStringLiteral("Choisissez un fournisseur."));
    if (draft.lines.isEmpty())
        return Result<PurchaseOrder>::fail(QStringLiteral("po.empty"),
                                           QStringLiteral("La commande est vide."));
    QSet<int> seen;
    for (const PoLine& line : draft.lines) {
        if (line.variantId <= 0 || line.qtyOrdered <= 0 || line.unitCost.isNegative())
            return Result<PurchaseOrder>::fail(
                QStringLiteral("po.line"),
                QStringLiteral("Ligne invalide (quantité > 0, coût >= 0)."));
        if (seen.contains(line.variantId))
            return Result<PurchaseOrder>::fail(
                QStringLiteral("po.duplicate"),
                QStringLiteral("Un même article ne peut apparaître qu'une fois."));
        seen.insert(line.variantId);
    }

    const int year = QDate::currentDate().year();
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<PurchaseOrder>::fail(QStringLiteral("po.tx"),
                                           database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<PurchaseOrder>::fail(std::move(code), std::move(message));
    };

    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) VALUES ('po', :y, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    counter.bindValue(QStringLiteral(":y"), year);
    if (!counter.exec())
        return fail(QStringLiteral("po.counter"), counter.lastError().text());
    QSqlQuery readCounter(database);
    readCounter.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters WHERE kind = 'po' AND year = :y"));
    readCounter.bindValue(QStringLiteral(":y"), year);
    if (!readCounter.exec() || !readCounter.next())
        return fail(QStringLiteral("po.counter"), readCounter.lastError().text());

    PurchaseOrder order;
    order.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    order.number = QStringLiteral("BC-%1-%2")
                       .arg(year)
                       .arg(readCounter.value(0).toInt(), 3, 10, QLatin1Char('0'));
    order.supplierId = draft.supplierId;
    order.totalCost = draft.totalCost();

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO purchase_orders (uuid, number, supplier_id, status, "
        "total_cost, note, user_id) "
        "VALUES (:uuid, :number, :supplier, 'draft', :total, :note, :user)"));
    insert.bindValue(QStringLiteral(":uuid"), order.uuid);
    insert.bindValue(QStringLiteral(":number"), order.number);
    insert.bindValue(QStringLiteral(":supplier"), draft.supplierId);
    insert.bindValue(QStringLiteral(":total"), order.totalCost.millimes());
    insert.bindValue(QStringLiteral(":note"),
                     draft.note.isEmpty() ? QVariant() : QVariant(draft.note));
    insert.bindValue(QStringLiteral(":user"),
                     draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    if (!insert.exec())
        return fail(QStringLiteral("po.insert"), insert.lastError().text());
    order.id = insert.lastInsertId().toInt();

    for (const PoLine& line : draft.lines) {
        QSqlQuery insertLine(database);
        insertLine.prepare(QStringLiteral(
            "INSERT INTO purchase_order_lines (po_id, variant_id, label, "
            "qty_ordered, unit_cost) "
            "VALUES (:po, :variant, :label, :qty, :cost)"));
        insertLine.bindValue(QStringLiteral(":po"), order.id);
        insertLine.bindValue(QStringLiteral(":variant"), line.variantId);
        // Un QString null serait bindé comme NULL SQL (colonne NOT NULL).
        insertLine.bindValue(QStringLiteral(":label"),
                             line.label.isNull() ? QStringLiteral("") : line.label);
        insertLine.bindValue(QStringLiteral(":qty"), line.qtyOrdered);
        insertLine.bindValue(QStringLiteral(":cost"), line.unitCost.millimes());
        if (!insertLine.exec())
            return fail(QStringLiteral("po.line"), insertLine.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<PurchaseOrder>::fail(QStringLiteral("po.commit"),
                                           database.lastError().text());
    }
    return Result<PurchaseOrder>::ok(std::move(order));
}

Result<QList<PoRow>> SqliteSupplierRepository::recentOrders(bool includeClosed,
                                                            int limit)
{
    QString sql = QStringLiteral(
        "SELECT po.id, po.number, po.created_at, s.name, po.status, "
        "  (SELECT count(*) FROM purchase_order_lines l WHERE l.po_id = po.id), "
        "  (SELECT COALESCE(SUM(qty_ordered), 0) FROM purchase_order_lines l "
        "   WHERE l.po_id = po.id), "
        "  (SELECT COALESCE(SUM(qty_received), 0) FROM purchase_order_lines l "
        "   WHERE l.po_id = po.id), "
        "  po.total_cost "
        "FROM purchase_orders po JOIN suppliers s ON s.id = po.supplier_id ");
    if (!includeClosed)
        sql += QStringLiteral("WHERE po.status NOT IN ('received', 'cancelled') ");
    sql += QStringLiteral("ORDER BY po.id DESC LIMIT :limit");

    QSqlQuery query(db());
    query.prepare(sql);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<PoRow>>::fail(QStringLiteral("po.recent"),
                                          query.lastError().text());
    QList<PoRow> rows;
    while (query.next()) {
        PoRow row;
        row.id = query.value(0).toInt();
        row.number = query.value(1).toString();
        row.createdAt = query.value(2).toString();
        row.supplierName = query.value(3).toString();
        row.status = query.value(4).toString();
        row.lineCount = query.value(5).toInt();
        row.qtyOrdered = query.value(6).toInt();
        row.qtyReceived = query.value(7).toInt();
        row.totalCost = Money::fromMillimes(query.value(8).toLongLong());
        rows.append(row);
    }
    return Result<QList<PoRow>>::ok(std::move(rows));
}

Result<PoDetail> SqliteSupplierRepository::orderDetail(int poId)
{
    QSqlQuery head(db());
    head.prepare(QStringLiteral(
        "SELECT po.number, s.name, po.status FROM purchase_orders po "
        "JOIN suppliers s ON s.id = po.supplier_id WHERE po.id = :id"));
    head.bindValue(QStringLiteral(":id"), poId);
    if (!head.exec() || !head.next())
        return Result<PoDetail>::fail(QStringLiteral("po.notFound"),
                                      QStringLiteral("Commande introuvable."));
    PoDetail detail;
    detail.id = poId;
    detail.number = head.value(0).toString();
    detail.supplierName = head.value(1).toString();
    detail.status = head.value(2).toString();

    QSqlQuery lines(db());
    lines.prepare(QStringLiteral(
        "SELECT variant_id, label, qty_ordered, qty_received, unit_cost "
        "FROM purchase_order_lines WHERE po_id = :id ORDER BY id"));
    lines.bindValue(QStringLiteral(":id"), poId);
    if (!lines.exec())
        return Result<PoDetail>::fail(QStringLiteral("po.lines"),
                                      lines.lastError().text());
    while (lines.next()) {
        PoLine line;
        line.variantId = lines.value(0).toInt();
        line.label = lines.value(1).toString();
        line.qtyOrdered = lines.value(2).toInt();
        line.qtyReceived = lines.value(3).toInt();
        line.unitCost = Money::fromMillimes(lines.value(4).toLongLong());
        detail.lines.append(line);
    }
    return Result<PoDetail>::ok(std::move(detail));
}

Result<void> SqliteSupplierRepository::setOrderStatus(int poId,
                                                      const QString& status)
{
    QSqlQuery current(db());
    current.prepare(QStringLiteral(
        "SELECT status FROM purchase_orders WHERE id = :id"));
    current.bindValue(QStringLiteral(":id"), poId);
    if (!current.exec() || !current.next())
        return Result<void>::fail(QStringLiteral("po.notFound"),
                                  QStringLiteral("Commande introuvable."));
    const QString from = current.value(0).toString();

    bool ok =
        (status == QLatin1String("sent") && from == QLatin1String("draft"))
        || (status == QLatin1String("cancelled")
            && (from == QLatin1String("draft") || from == QLatin1String("sent")));

    // Correction d'une fausse manipulation : retour au brouillon d'une
    // commande envoyée ou annulée, tant que rien n'a été reçu.
    if (status == QLatin1String("draft")
        && (from == QLatin1String("sent") || from == QLatin1String("cancelled")
            || from == QLatin1String("partial"))) {
        QSqlQuery received(db());
        received.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(qty_received), 0) FROM purchase_order_lines "
            "WHERE po_id = :id"));
        received.bindValue(QStringLiteral(":id"), poId);
        if (received.exec() && received.next()
            && received.value(0).toInt() == 0)
            ok = true;
        else
            return Result<void>::fail(
                QStringLiteral("po.received"),
                QStringLiteral("Impossible : des quantités ont déjà été reçues."));
    }

    if (!ok)
        return Result<void>::fail(
            QStringLiteral("po.transition"),
            QStringLiteral("Transition de statut non autorisée."));

    QSqlQuery update(db());
    update.prepare(QStringLiteral(
        "UPDATE purchase_orders SET status = :status WHERE id = :id"));
    update.bindValue(QStringLiteral(":status"), status);
    update.bindValue(QStringLiteral(":id"), poId);
    if (!update.exec())
        return Result<void>::fail(QStringLiteral("po.status"),
                                  update.lastError().text());
    return Result<void>::ok();
}

Result<int> SqliteSupplierRepository::receiveOrder(const PoReceiptDraft& draft)
{
    if (draft.locationId <= 0)
        return Result<int>::fail(QStringLiteral("po.location"),
                                 QStringLiteral("Choisissez un emplacement."));

    // État de la commande + coût et reste par article
    QSqlDatabase database = db();
    QSqlQuery head(database);
    head.prepare(QStringLiteral(
        "SELECT supplier_id, status FROM purchase_orders WHERE id = :id"));
    head.bindValue(QStringLiteral(":id"), draft.poId);
    if (!head.exec() || !head.next())
        return Result<int>::fail(QStringLiteral("po.notFound"),
                                 QStringLiteral("Commande introuvable."));
    const int supplierId = head.value(0).toInt();
    const QString status = head.value(1).toString();
    if (status != QLatin1String("sent") && status != QLatin1String("partial"))
        return Result<int>::fail(
            QStringLiteral("po.notReceivable"),
            QStringLiteral("Seule une commande envoyée peut être reçue."));

    QSqlQuery lines(database);
    lines.prepare(QStringLiteral(
        "SELECT variant_id, qty_ordered - qty_received, unit_cost "
        "FROM purchase_order_lines WHERE po_id = :id"));
    lines.bindValue(QStringLiteral(":id"), draft.poId);
    if (!lines.exec())
        return Result<int>::fail(QStringLiteral("po.lines"), lines.lastError().text());
    QHash<int, int> remaining;   // variantId -> reste
    QHash<int, qint64> unitCost; // variantId -> coût commandé
    while (lines.next()) {
        const int variantId = lines.value(0).toInt();
        remaining.insert(variantId, lines.value(1).toInt());
        unitCost.insert(variantId, lines.value(2).toLongLong());
    }

    // Valide les quantités reçues (> 0, <= reste)
    QList<PoReceiptLine> toReceive;
    for (const PoReceiptLine& line : draft.lines) {
        if (line.qty <= 0)
            continue;
        if (!remaining.contains(line.variantId))
            return Result<int>::fail(
                QStringLiteral("po.unknownLine"),
                QStringLiteral("Article absent de la commande."));
        if (line.qty > remaining.value(line.variantId))
            return Result<int>::fail(
                QStringLiteral("po.overReceive"),
                QStringLiteral("Quantité reçue supérieure au reste à recevoir."));
        toReceive.append(line);
    }
    if (toReceive.isEmpty())
        return Result<int>::fail(QStringLiteral("po.nothing"),
                                 QStringLiteral("Aucune quantité à recevoir."));

    if (!database.transaction())
        return Result<int>::fail(QStringLiteral("po.tx"), database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<int>::fail(std::move(code), std::move(message));
    };

    qint64 total = 0;
    for (const PoReceiptLine& line : toReceive)
        total += static_cast<qint64>(line.qty) * unitCost.value(line.variantId);

    QSqlQuery insertReceipt(database);
    insertReceipt.prepare(QStringLiteral(
        "INSERT INTO receipts (uuid, supplier_id, location_id, total_cost, "
        "note, user_id, po_id) "
        "VALUES (:uuid, :supplier, :location, :total, :note, :user, :po)"));
    insertReceipt.bindValue(QStringLiteral(":uuid"),
                            QUuid::createUuid().toString(QUuid::WithoutBraces));
    insertReceipt.bindValue(QStringLiteral(":supplier"),
                            supplierId > 0 ? QVariant(supplierId) : QVariant());
    insertReceipt.bindValue(QStringLiteral(":location"), draft.locationId);
    insertReceipt.bindValue(QStringLiteral(":total"), total);
    insertReceipt.bindValue(QStringLiteral(":note"),
                            draft.note.isEmpty() ? QVariant() : QVariant(draft.note));
    insertReceipt.bindValue(QStringLiteral(":user"),
                            draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    insertReceipt.bindValue(QStringLiteral(":po"), draft.poId);
    if (!insertReceipt.exec())
        return fail(QStringLiteral("po.receipt"), insertReceipt.lastError().text());
    const int receiptId = insertReceipt.lastInsertId().toInt();

    for (const PoReceiptLine& line : toReceive) {
        const qint64 cost = unitCost.value(line.variantId);

        QSqlQuery insertLine(database);
        insertLine.prepare(QStringLiteral(
            "INSERT INTO receipt_lines (receipt_id, variant_id, qty, unit_cost) "
            "VALUES (:receipt, :variant, :qty, :cost)"));
        insertLine.bindValue(QStringLiteral(":receipt"), receiptId);
        insertLine.bindValue(QStringLiteral(":variant"), line.variantId);
        insertLine.bindValue(QStringLiteral(":qty"), line.qty);
        insertLine.bindValue(QStringLiteral(":cost"), cost);
        if (!insertLine.exec())
            return fail(QStringLiteral("po.receiptLine"), insertLine.lastError().text());

        if (const auto avg = updateAverageCost(line.variantId, line.qty, cost); !avg)
            return fail(avg.error().code, avg.error().message);

        StockMove entry;
        entry.kind = MoveKind::In;
        entry.variantId = line.variantId;
        entry.toLocationId = draft.locationId;
        entry.qty = line.qty;
        entry.refKind = QStringLiteral("purchase");
        entry.refId = receiptId;
        entry.userId = draft.userId;
        if (const auto moved = m_stock.recordMove(entry, /*ownTransaction=*/false);
            !moved)
            return fail(moved.error().code, moved.error().message);

        QSqlQuery bump(database);
        bump.prepare(QStringLiteral(
            "UPDATE purchase_order_lines SET qty_received = qty_received + :qty "
            "WHERE po_id = :po AND variant_id = :variant"));
        bump.bindValue(QStringLiteral(":qty"), line.qty);
        bump.bindValue(QStringLiteral(":po"), draft.poId);
        bump.bindValue(QStringLiteral(":variant"), line.variantId);
        if (!bump.exec())
            return fail(QStringLiteral("po.bump"), bump.lastError().text());
    }

    // Statut recalculé : reçue si plus aucune ligne en reste, sinon partielle
    QSqlQuery pending(database);
    pending.prepare(QStringLiteral(
        "SELECT count(*) FROM purchase_order_lines "
        "WHERE po_id = :po AND qty_received < qty_ordered"));
    pending.bindValue(QStringLiteral(":po"), draft.poId);
    if (!pending.exec() || !pending.next())
        return fail(QStringLiteral("po.pending"), pending.lastError().text());
    const QString newStatus = pending.value(0).toInt() == 0
        ? QStringLiteral("received")
        : QStringLiteral("partial");

    QSqlQuery setStatus(database);
    setStatus.prepare(QStringLiteral(
        "UPDATE purchase_orders SET status = :status WHERE id = :po"));
    setStatus.bindValue(QStringLiteral(":status"), newStatus);
    setStatus.bindValue(QStringLiteral(":po"), draft.poId);
    if (!setStatus.exec())
        return fail(QStringLiteral("po.setStatus"), setStatus.lastError().text());

    if (!database.commit()) {
        database.rollback();
        return Result<int>::fail(QStringLiteral("po.commit"),
                                 database.lastError().text());
    }
    return Result<int>::ok(receiptId);
}

} // namespace nursera
