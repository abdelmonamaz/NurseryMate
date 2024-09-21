#include "repositories/sqlite/sqlite_sale_repository.h"

#include <QDate>
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

QString methodToString(PaymentMethod method)
{
    switch (method) {
    case PaymentMethod::Cheque: return QStringLiteral("cheque");
    case PaymentMethod::Transfer: return QStringLiteral("transfer");
    case PaymentMethod::Cash: break;
    }
    return QStringLiteral("cash");
}

} // namespace

SqliteSaleRepository::SqliteSaleRepository(QString connectionName,
                                           IStockRepository& stock)
    : m_connectionName(std::move(connectionName))
    , m_stock(stock)
{
}

Result<QString> SqliteSaleRepository::nextTicketNumber()
{
    const int year = QDate::currentDate().year();
    QSqlDatabase database = db();

    // Réserve le compteur (créé à 1 s'il n'existe pas encore)
    QSqlQuery upsert(database);
    upsert.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) "
        "VALUES ('ticket', :year, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    upsert.bindValue(QStringLiteral(":year"), year);
    if (!upsert.exec())
        return Result<QString>::fail(QStringLiteral("sale.counter"),
                                     upsert.lastError().text());

    QSqlQuery read(database);
    read.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters "
        "WHERE kind = 'ticket' AND year = :year"));
    read.bindValue(QStringLiteral(":year"), year);
    if (!read.exec() || !read.next())
        return Result<QString>::fail(QStringLiteral("sale.counter"),
                                     read.lastError().text());

    return Result<QString>::ok(QStringLiteral("T-%1-%2")
                                   .arg(year)
                                   .arg(read.value(0).toInt(), 5, 10,
                                        QLatin1Char('0')));
}

Result<Sale> SqliteSaleRepository::record(const SaleDraft& draft)
{
    if (draft.lines.isEmpty())
        return Result<Sale>::fail(QStringLiteral("sale.empty"),
                                  QStringLiteral("Le panier est vide."));
    if (draft.total().isNegative())
        return Result<Sale>::fail(
            QStringLiteral("sale.discount"),
            QStringLiteral("La remise dépasse le total de la vente."));
    if (draft.stockLocationId <= 0)
        return Result<Sale>::fail(
            QStringLiteral("sale.location"),
            QStringLiteral("Emplacement de caisse non configuré (RG-04.a)."));

    if (draft.onCredit && draft.customerId <= 0)
        return Result<Sale>::fail(
            QStringLiteral("sale.credit"),
            QStringLiteral("Une vente à crédit exige un client identifié (F04-05)."));

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<Sale>::fail(QStringLiteral("sale.tx"),
                                  database.lastError().text());

    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<Sale>::fail(std::move(code), std::move(message));
    };

    // Plafond de crédit (RG-06.b) : encours actuel + vente <= plafond
    if (draft.onCredit) {
        QSqlQuery credit(database);
        credit.prepare(QStringLiteral(
            "SELECT c.credit_limit, "
            "COALESCE((SELECT SUM(total) FROM sales s "
            "          WHERE s.customer_id = c.id AND s.status = 'completed'), 0) "
            "- COALESCE((SELECT SUM(amount) FROM payments p "
            "            WHERE p.customer_id = c.id "
            "            AND (p.sale_id IS NULL OR (SELECT status FROM sales sx "
            "                 WHERE sx.id = p.sale_id) = 'completed')), 0) "
            "FROM customers c WHERE c.id = :id AND c.active = 1"));
        credit.bindValue(QStringLiteral(":id"), draft.customerId);
        if (!credit.exec() || !credit.next())
            return fail(QStringLiteral("sale.customer"),
                        QStringLiteral("Client introuvable."));
        if (!credit.value(0).isNull()) {
            const qint64 limit = credit.value(0).toLongLong();
            const qint64 balance = credit.value(1).toLongLong();
            if (balance + draft.total().millimes() > limit)
                return fail(QStringLiteral("sale.creditLimit"),
                            QStringLiteral("Plafond de crédit dépassé "
                                           "(encours %1 + vente %2 > plafond %3 DT) "
                                           "— validation Gérant requise.")
                                .arg(Money::fromMillimes(balance)
                                         .toDisplayString(QLocale()))
                                .arg(draft.total().toDisplayString(QLocale()))
                                .arg(Money::fromMillimes(limit)
                                         .toDisplayString(QLocale())));
        }
    }

    // 1) Numéro sans trou, réservé dans la même transaction (RG-04.b)
    const auto number = nextTicketNumber();
    if (!number)
        return fail(number.error().code, number.error().message);

    // 2) TVA contenue dans le TTC, ligne à ligne
    Money vatTotal;
    for (const SaleLine& line : draft.lines)
        vatTotal = vatTotal + line.lineTotal().vatFromTtc(line.vatRatePercent);

    Sale sale;
    sale.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sale.number = number.value();
    sale.total = draft.total();

    // 3) Entête de vente — à crédit : paid_total = 0, l'encours client
    //    porte le solde (F04-05)
    QSqlQuery insertSale(database);
    insertSale.prepare(QStringLiteral(
        "INSERT INTO sales (uuid, number, subtotal, discount, vat_total, total, "
        "paid_total, user_id, customer_id) "
        "VALUES (:uuid, :number, :subtotal, :discount, :vat, :total, :paid, "
        ":user, :customer)"));
    insertSale.bindValue(QStringLiteral(":uuid"), sale.uuid);
    insertSale.bindValue(QStringLiteral(":number"), sale.number);
    insertSale.bindValue(QStringLiteral(":subtotal"), draft.subtotal().millimes());
    insertSale.bindValue(QStringLiteral(":discount"), draft.globalDiscount.millimes());
    insertSale.bindValue(QStringLiteral(":vat"), vatTotal.millimes());
    insertSale.bindValue(QStringLiteral(":total"), sale.total.millimes());
    insertSale.bindValue(QStringLiteral(":paid"),
                         draft.onCredit ? 0 : sale.total.millimes());
    insertSale.bindValue(QStringLiteral(":user"),
                         draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    insertSale.bindValue(QStringLiteral(":customer"),
                         draft.customerId > 0 ? QVariant(draft.customerId)
                                              : QVariant());
    if (!insertSale.exec())
        return fail(QStringLiteral("sale.insert"), insertSale.lastError().text());
    sale.id = insertSale.lastInsertId().toInt();

    // 4) Lignes (snapshot libellé + prix) et sorties de stock (RG-04.a)
    for (const SaleLine& line : draft.lines) {
        QSqlQuery insertLine(database);
        insertLine.prepare(QStringLiteral(
            "INSERT INTO sale_lines (sale_id, variant_id, label_snapshot, qty, "
            "unit_price, vat_rate, discount_bp, line_total) "
            "VALUES (:sale, :variant, :label, :qty, :price, :vat, :discount, :total)"));
        insertLine.bindValue(QStringLiteral(":sale"), sale.id);
        // Ligne libre / prestation (devis chargé en caisse) : variant NULL.
        insertLine.bindValue(QStringLiteral(":variant"),
                             line.variantId > 0 ? QVariant(line.variantId)
                                                : QVariant());
        insertLine.bindValue(QStringLiteral(":label"), line.label);
        insertLine.bindValue(QStringLiteral(":qty"), line.qty);
        insertLine.bindValue(QStringLiteral(":price"), line.unitPrice.millimes());
        insertLine.bindValue(QStringLiteral(":vat"), line.vatRatePercent);
        insertLine.bindValue(QStringLiteral(":discount"), line.discountBp);
        insertLine.bindValue(QStringLiteral(":total"), line.lineTotal().millimes());
        if (!insertLine.exec())
            return fail(QStringLiteral("sale.line"), insertLine.lastError().text());

        // Une prestation ne touche jamais le stock.
        if (line.variantId <= 0)
            continue;

        StockMove out;
        out.kind = MoveKind::Out;
        out.variantId = line.variantId;
        out.fromLocationId = draft.stockLocationId;
        out.qty = line.qty;
        out.refKind = QStringLiteral("sale");
        out.refId = sale.id;
        out.userId = draft.userId;
        if (const auto moved = m_stock.recordMove(out, /*ownTransaction=*/false);
            !moved)
            return fail(moved.error().code, moved.error().message);
    }

    // 5) Paiement — aucun encaissement pour une vente à crédit ;
    //    customer_id posé pour le calcul d'encours (doc 04)
    if (!draft.onCredit) {
        QSqlQuery insertPayment(database);
        insertPayment.prepare(QStringLiteral(
            "INSERT INTO payments (uuid, sale_id, method, amount, cheque_number, "
            "cheque_bank, cheque_due, user_id, customer_id) "
            "VALUES (:uuid, :sale, :method, :amount, :cheque_number, :cheque_bank, "
            ":cheque_due, :user, :customer)"));
        insertPayment.bindValue(QStringLiteral(":uuid"),
                                QUuid::createUuid().toString(QUuid::WithoutBraces));
        insertPayment.bindValue(QStringLiteral(":sale"), sale.id);
        insertPayment.bindValue(QStringLiteral(":method"), methodToString(draft.method));
        insertPayment.bindValue(QStringLiteral(":amount"), sale.total.millimes());
        insertPayment.bindValue(QStringLiteral(":cheque_number"),
                                draft.chequeNumber.isEmpty() ? QVariant()
                                                             : QVariant(draft.chequeNumber));
        insertPayment.bindValue(QStringLiteral(":cheque_bank"),
                                draft.chequeBank.isEmpty() ? QVariant()
                                                           : QVariant(draft.chequeBank));
        insertPayment.bindValue(QStringLiteral(":cheque_due"),
                                draft.chequeDue.isEmpty() ? QVariant()
                                                          : QVariant(draft.chequeDue));
        insertPayment.bindValue(QStringLiteral(":user"),
                                draft.userId > 0 ? QVariant(draft.userId) : QVariant());
        insertPayment.bindValue(QStringLiteral(":customer"),
                                draft.customerId > 0 ? QVariant(draft.customerId)
                                                     : QVariant());
        if (!insertPayment.exec())
            return fail(QStringLiteral("sale.payment"),
                        insertPayment.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<Sale>::fail(QStringLiteral("sale.commit"),
                                  database.lastError().text());
    }
    return Result<Sale>::ok(std::move(sale));
}

Result<SaleDetails> SqliteSaleRepository::details(int saleId)
{
    QSqlQuery header(db());
    header.prepare(QStringLiteral(
        "SELECT s.number, s.created_at, s.subtotal, s.discount, s.vat_total, "
        "       s.total, COALESCE(u.display_name, ''), "
        "       COALESCE((SELECT group_concat(DISTINCT p.method) "
        "                 FROM payments p WHERE p.sale_id = s.id), '') "
        "FROM sales s LEFT JOIN users u ON u.id = s.user_id "
        "WHERE s.id = :id"));
    header.bindValue(QStringLiteral(":id"), saleId);
    if (!header.exec())
        return Result<SaleDetails>::fail(QStringLiteral("sale.details"),
                                         header.lastError().text());
    if (!header.next())
        return Result<SaleDetails>::fail(QStringLiteral("sale.notFound"),
                                         QStringLiteral("Vente introuvable."));

    SaleDetails details;
    details.id = saleId;
    details.number = header.value(0).toString();
    details.createdAt = header.value(1).toString();
    details.subtotal = Money::fromMillimes(header.value(2).toLongLong());
    details.discount = Money::fromMillimes(header.value(3).toLongLong());
    details.vatTotal = Money::fromMillimes(header.value(4).toLongLong());
    details.total = Money::fromMillimes(header.value(5).toLongLong());
    details.userName = header.value(6).toString();
    details.method = header.value(7).toString();

    QSqlQuery lines(db());
    lines.prepare(QStringLiteral(
        "SELECT label_snapshot, qty, unit_price, line_total "
        "FROM sale_lines WHERE sale_id = :id ORDER BY id"));
    lines.bindValue(QStringLiteral(":id"), saleId);
    if (!lines.exec())
        return Result<SaleDetails>::fail(QStringLiteral("sale.details"),
                                         lines.lastError().text());
    while (lines.next()) {
        SaleDetailLine line;
        line.label = lines.value(0).toString();
        line.qty = lines.value(1).toInt();
        line.unitPrice = Money::fromMillimes(lines.value(2).toLongLong());
        line.lineTotal = Money::fromMillimes(lines.value(3).toLongLong());
        details.lines.append(line);
    }
    return Result<SaleDetails>::ok(std::move(details));
}

Result<void> SqliteSaleRepository::cancel(int saleId, const QString& reason,
                                          int userId)
{
    if (reason.trimmed().isEmpty())
        return Result<void>::fail(QStringLiteral("sale.cancelReason"),
                                  QStringLiteral("Le motif d'annulation est "
                                                 "obligatoire (F04-08)."));

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("sale.tx"),
                                  database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<void>::fail(std::move(code), std::move(message));
    };

    QSqlQuery sale(database);
    sale.prepare(QStringLiteral(
        "SELECT uuid, status, date(created_at) = date('now') "
        "FROM sales WHERE id = :id"));
    sale.bindValue(QStringLiteral(":id"), saleId);
    if (!sale.exec() || !sale.next())
        return fail(QStringLiteral("sale.notFound"),
                    QStringLiteral("Vente introuvable."));
    if (sale.value(1).toString() != QLatin1String("completed"))
        return fail(QStringLiteral("sale.cancelled"),
                    QStringLiteral("Vente déjà annulée."));
    if (!sale.value(2).toBool())
        return fail(QStringLiteral("sale.tooLate"),
                    QStringLiteral("Annulation possible le jour même uniquement "
                                   "— au-delà, passez par un avoir (V1)."));
    const QString saleUuid = sale.value(0).toString();

    // Contre-mouvements : ré-entrée sur l'emplacement d'origine, uuid v5
    // déterministe -> un rejeu après crash ne double jamais (doc 02 §5.1).
    static const QUuid kCancelNamespace(
        QStringLiteral("{c3a9d2f1-8e4b-4c6a-b5d7-1f2e3a4b5c6d}"));
    QSqlQuery outs(database);
    outs.prepare(QStringLiteral(
        "SELECT variant_id, from_location_id, qty FROM stock_moves "
        "WHERE ref_kind = 'sale' AND ref_id = :id"));
    outs.bindValue(QStringLiteral(":id"), saleId);
    if (!outs.exec())
        return fail(QStringLiteral("sale.cancelMoves"), outs.lastError().text());
    while (outs.next()) {
        StockMove back;
        back.kind = MoveKind::In;
        back.variantId = outs.value(0).toInt();
        back.toLocationId = outs.value(1).toInt();
        back.qty = outs.value(2).toInt();
        back.refKind = QStringLiteral("sale_cancel");
        back.refId = saleId;
        back.reason = reason.trimmed();
        back.userId = userId;
        back.uuid = QUuid::createUuidV5(
                        kCancelNamespace,
                        QStringLiteral("%1:%2").arg(saleUuid).arg(back.variantId))
                        .toString(QUuid::WithoutBraces);
        if (const auto moved = m_stock.recordMove(back, /*ownTransaction=*/false);
            !moved)
            return fail(moved.error().code, moved.error().message);
    }

    QSqlQuery update(database);
    update.prepare(QStringLiteral(
        "UPDATE sales SET status = 'cancelled', cancelled_at = datetime('now'), "
        "cancel_reason = :reason WHERE id = :id AND status = 'completed'"));
    update.bindValue(QStringLiteral(":reason"), reason.trimmed());
    update.bindValue(QStringLiteral(":id"), saleId);
    if (!update.exec() || update.numRowsAffected() == 0)
        return fail(QStringLiteral("sale.cancel"), update.lastError().text());

    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("sale.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<CreditNote> SqliteSaleRepository::createCreditNote(
    const CreditNoteDraft& draft)
{
    if (draft.reason.trimmed().isEmpty())
        return Result<CreditNote>::fail(
            QStringLiteral("credit.reason"),
            QStringLiteral("Le motif de l'avoir est obligatoire."));
    if (draft.restock && draft.stockLocationId <= 0)
        return Result<CreditNote>::fail(
            QStringLiteral("credit.location"),
            QStringLiteral("Choisissez l'emplacement du retour en stock."));

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<CreditNote>::fail(QStringLiteral("credit.tx"),
                                        database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<CreditNote>::fail(std::move(code), std::move(message));
    };

    QSqlQuery sale(database);
    sale.prepare(QStringLiteral(
        "SELECT uuid, status, total, subtotal, "
        "COALESCE((SELECT SUM(cn.total) FROM credit_notes cn "
        "          WHERE cn.sale_id = sales.id), 0) "
        "FROM sales WHERE id = :id"));
    sale.bindValue(QStringLiteral(":id"), draft.saleId);
    if (!sale.exec() || !sale.next())
        return fail(QStringLiteral("credit.notFound"),
                    QStringLiteral("Vente introuvable."));
    if (sale.value(1).toString() != QLatin1String("completed"))
        return fail(QStringLiteral("credit.cancelled"),
                    QStringLiteral("Vente annulée — un avoir ne s'applique "
                                   "qu'à une vente valide."));
    const QString saleUuid = sale.value(0).toString();
    const qint64 saleTotal = sale.value(2).toLongLong();
    const qint64 saleSubtotal = sale.value(3).toLongLong();
    const qint64 alreadyRefunded = sale.value(4).toLongLong();

    // Lignes remboursables : vendu − déjà remboursé par les avoirs passés.
    QSqlQuery lines(database);
    lines.prepare(QStringLiteral(
        "SELECT sl.id, sl.variant_id, sl.label_snapshot, sl.qty, "
        "       sl.unit_price, "
        "       COALESCE((SELECT SUM(cnl.qty) FROM credit_note_lines cnl "
        "                 WHERE cnl.sale_line_id = sl.id), 0) "
        "FROM sale_lines sl WHERE sl.sale_id = :id ORDER BY sl.id"));
    lines.bindValue(QStringLiteral(":id"), draft.saleId);
    if (!lines.exec())
        return fail(QStringLiteral("credit.lines"), lines.lastError().text());

    struct Refund { int saleLineId; int variantId; QString label;
                    int qty; qint64 unitPrice; int remaining; };
    QList<Refund> refundable;
    while (lines.next()) {
        Refund refund;
        refund.saleLineId = lines.value(0).toInt();
        refund.variantId = lines.value(1).toInt();
        refund.label = lines.value(2).toString();
        refund.unitPrice = lines.value(4).toLongLong();
        refund.remaining = lines.value(3).toInt() - lines.value(5).toInt();
        refund.qty = 0;
        refundable.append(refund);
    }

    // Sélection : draft.lines vide = TOUT le restant ; sinon quantités
    // choisies, bornées par le restant de chaque ligne.
    bool anything = false;
    if (draft.lines.isEmpty()) {
        for (Refund& refund : refundable) {
            refund.qty = refund.remaining;
            anything = anything || refund.qty > 0;
        }
    } else {
        for (const CreditNoteLineDraft& asked : draft.lines) {
            bool found = false;
            for (Refund& refund : refundable) {
                if (refund.saleLineId != asked.saleLineId)
                    continue;
                found = true;
                if (asked.qty < 0 || asked.qty > refund.remaining)
                    return fail(
                        QStringLiteral("credit.qty"),
                        QStringLiteral("%1 : %2 demandé, %3 remboursable.")
                            .arg(refund.label)
                            .arg(asked.qty)
                            .arg(refund.remaining));
                refund.qty = asked.qty;
                anything = anything || asked.qty > 0;
            }
            if (!found)
                return fail(QStringLiteral("credit.line"),
                            QStringLiteral("Ligne de vente inconnue."));
        }
    }
    if (!anything)
        return fail(QStringLiteral("credit.nothing"),
                    QStringLiteral("Rien à rembourser sur cette vente."));

    // Total de l'avoir : lignes remboursées, remise globale reprise au
    // prorata. Si plus rien ne reste après cet avoir, réconciliation
    // exacte : cumul des avoirs == total de la vente, au millime.
    qint64 linesSum = 0;
    bool nothingLeftAfter = true;
    for (const Refund& refund : refundable) {
        linesSum += qint64(refund.qty) * refund.unitPrice;
        if (refund.remaining - refund.qty > 0)
            nothingLeftAfter = false;
    }
    qint64 totalMillimes = nothingLeftAfter
        ? saleTotal - alreadyRefunded
        : (saleSubtotal > 0
               ? (linesSum * saleTotal + saleSubtotal / 2) / saleSubtotal
               : linesSum);

    // Numéro AV-AAAA-NNN sans trou (doc_counters kind='credit_note')
    const int year = QDate::currentDate().year();
    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) "
        "VALUES ('credit_note', :y, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    counter.bindValue(QStringLiteral(":y"), year);
    if (!counter.exec())
        return fail(QStringLiteral("credit.counter"), counter.lastError().text());
    QSqlQuery readCounter(database);
    readCounter.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters "
        "WHERE kind = 'credit_note' AND year = :y"));
    readCounter.bindValue(QStringLiteral(":y"), year);
    if (!readCounter.exec() || !readCounter.next())
        return fail(QStringLiteral("credit.counter"),
                    readCounter.lastError().text());

    CreditNote note;
    note.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    note.number = QStringLiteral("AV-%1-%2")
                      .arg(year)
                      .arg(readCounter.value(0).toInt(), 3, 10, QLatin1Char('0'));
    note.total = Money::fromMillimes(totalMillimes);

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO credit_notes (uuid, number, sale_id, reason, restock, "
        "refund_method, total, user_id) "
        "VALUES (:uuid, :number, :sale, :reason, :restock, :method, :total, "
        ":user)"));
    insert.bindValue(QStringLiteral(":uuid"), note.uuid);
    insert.bindValue(QStringLiteral(":number"), note.number);
    insert.bindValue(QStringLiteral(":sale"), draft.saleId);
    insert.bindValue(QStringLiteral(":reason"), draft.reason.trimmed());
    insert.bindValue(QStringLiteral(":restock"), draft.restock ? 1 : 0);
    insert.bindValue(QStringLiteral(":method"), draft.refundMethod);
    insert.bindValue(QStringLiteral(":total"), note.total.millimes());
    insert.bindValue(QStringLiteral(":user"),
                     draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    if (!insert.exec())
        return fail(QStringLiteral("credit.insert"), insert.lastError().text());
    note.id = insert.lastInsertId().toInt();

    // Lignes remboursées par CET avoir (snapshot) + retour en stock
    // idempotent (uuid v5 sur l'uuid de l'AVOIR — plusieurs avoirs par
    // vente peuvent restocker la même variante).
    static const QUuid kCreditNamespace(
        QStringLiteral("{7f2b9c4d-3a1e-4f5b-8c6d-2e9a0b1c3d4e}"));
    for (const auto& refund : refundable) {
        if (refund.qty <= 0)
            continue;
        QSqlQuery lineInsert(database);
        lineInsert.prepare(QStringLiteral(
            "INSERT INTO credit_note_lines (credit_note_id, sale_line_id, "
            "variant_id, label_snapshot, qty, unit_price, line_total) "
            "VALUES (:note, :line, :variant, :label, :qty, :price, :total)"));
        lineInsert.bindValue(QStringLiteral(":note"), note.id);
        lineInsert.bindValue(QStringLiteral(":line"), refund.saleLineId);
        lineInsert.bindValue(QStringLiteral(":variant"),
                             refund.variantId > 0 ? QVariant(refund.variantId)
                                                  : QVariant());
        lineInsert.bindValue(QStringLiteral(":label"), refund.label);
        lineInsert.bindValue(QStringLiteral(":qty"), refund.qty);
        lineInsert.bindValue(QStringLiteral(":price"), refund.unitPrice);
        lineInsert.bindValue(QStringLiteral(":total"),
                             qint64(refund.qty) * refund.unitPrice);
        if (!lineInsert.exec())
            return fail(QStringLiteral("credit.lineInsert"),
                        lineInsert.lastError().text());

        // Prestations libres (variant 0) : aucun mouvement de stock.
        if (draft.restock && refund.variantId > 0) {
            StockMove back;
            back.kind = MoveKind::In;
            back.variantId = refund.variantId;
            back.toLocationId = draft.stockLocationId;
            back.qty = refund.qty;
            back.refKind = QStringLiteral("credit_note");
            back.refId = note.id;
            back.reason = draft.reason.trimmed();
            back.userId = draft.userId;
            back.uuid = QUuid::createUuidV5(
                            kCreditNamespace,
                            QStringLiteral("%1:%2:%3")
                                .arg(note.uuid, saleUuid)
                                .arg(refund.saleLineId))
                            .toString(QUuid::WithoutBraces);
            if (const auto moved =
                    m_stock.recordMove(back, /*ownTransaction=*/false);
                !moved)
                return fail(moved.error().code, moved.error().message);
        }
    }

    if (!database.commit()) {
        database.rollback();
        return Result<CreditNote>::fail(QStringLiteral("credit.commit"),
                                        database.lastError().text());
    }
    return Result<CreditNote>::ok(std::move(note));
}

Result<QList<RefundableLine>> SqliteSaleRepository::refundableLines(int saleId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT sl.id, COALESCE(sl.variant_id, 0), sl.label_snapshot, "
        "       sl.qty, sl.unit_price, "
        "       COALESCE((SELECT SUM(cnl.qty) FROM credit_note_lines cnl "
        "                 WHERE cnl.sale_line_id = sl.id), 0) "
        "FROM sale_lines sl WHERE sl.sale_id = :id ORDER BY sl.id"));
    query.bindValue(QStringLiteral(":id"), saleId);
    if (!query.exec())
        return Result<QList<RefundableLine>>::fail(
            QStringLiteral("credit.refundable"), query.lastError().text());

    QList<RefundableLine> rows;
    while (query.next()) {
        RefundableLine row;
        row.saleLineId = query.value(0).toInt();
        row.variantId = query.value(1).toInt();
        row.label = query.value(2).toString();
        row.qtySold = query.value(3).toInt();
        row.unitPrice = Money::fromMillimes(query.value(4).toLongLong());
        row.qtyRefunded = query.value(5).toInt();
        rows.append(row);
    }
    return Result<QList<RefundableLine>>::ok(std::move(rows));
}

Result<CreditNoteDetails> SqliteSaleRepository::creditNoteDetails(int saleId)
{
    // Dernier avoir de la vente (chaque PDF est généré à l'émission — la
    // réimpression depuis le journal vise le plus récent).
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT cn.number, cn.created_at, cn.reason, cn.restock, "
        "       cn.refund_method, cn.total, COALESCE(u.display_name, ''), "
        "       cn.id "
        "FROM credit_notes cn LEFT JOIN users u ON u.id = cn.user_id "
        "WHERE cn.sale_id = :id ORDER BY cn.id DESC LIMIT 1"));
    query.bindValue(QStringLiteral(":id"), saleId);
    if (!query.exec())
        return Result<CreditNoteDetails>::fail(QStringLiteral("credit.details"),
                                               query.lastError().text());
    if (!query.next())
        return Result<CreditNoteDetails>::fail(
            QStringLiteral("credit.notFound"),
            QStringLiteral("Aucun avoir pour cette vente."));

    CreditNoteDetails note;
    note.number = query.value(0).toString();
    note.createdAt = query.value(1).toString();
    note.reason = query.value(2).toString();
    note.restock = query.value(3).toBool();
    note.refundMethod = query.value(4).toString();
    note.total = Money::fromMillimes(query.value(5).toLongLong());
    note.userName = query.value(6).toString();
    const int noteId = query.value(7).toInt();

    QSqlQuery lines(db());
    lines.prepare(QStringLiteral(
        "SELECT label_snapshot, qty, unit_price, line_total "
        "FROM credit_note_lines WHERE credit_note_id = :id ORDER BY id"));
    lines.bindValue(QStringLiteral(":id"), noteId);
    if (!lines.exec())
        return Result<CreditNoteDetails>::fail(QStringLiteral("credit.details"),
                                               lines.lastError().text());
    while (lines.next()) {
        SaleDetailLine line;
        line.label = lines.value(0).toString();
        line.qty = lines.value(1).toInt();
        line.unitPrice = Money::fromMillimes(lines.value(2).toLongLong());
        line.lineTotal = Money::fromMillimes(lines.value(3).toLongLong());
        note.lines.append(line);
    }

    auto sale = details(saleId);
    if (!sale)
        return Result<CreditNoteDetails>::fail(sale.error().code,
                                               sale.error().message);
    note.sale = std::move(sale.value());
    return Result<CreditNoteDetails>::ok(std::move(note));
}

Result<QList<SaleJournalRow>> SqliteSaleRepository::todayJournal()
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT s.id, s.number, s.created_at, s.total, s.status, "
            "       COALESCE(u.display_name, ''), "
            "       COALESCE((SELECT group_concat(DISTINCT p.method) "
            "                 FROM payments p WHERE p.sale_id = s.id), ''), "
            "       COALESCE((SELECT cn.number FROM credit_notes cn "
            "                 WHERE cn.sale_id = s.id "
            "                 ORDER BY cn.id DESC LIMIT 1), ''), "
            "       (SELECT COUNT(*) FROM credit_notes cn "
            "        WHERE cn.sale_id = s.id), "
            "       COALESCE((SELECT SUM(cn.total) FROM credit_notes cn "
            "                 WHERE cn.sale_id = s.id), 0) "
            "FROM sales s LEFT JOIN users u ON u.id = s.user_id "
            "WHERE date(s.created_at) = date('now') "
            "ORDER BY s.id DESC")))
        return Result<QList<SaleJournalRow>>::fail(QStringLiteral("sale.journal"),
                                                   query.lastError().text());

    QList<SaleJournalRow> rows;
    while (query.next()) {
        SaleJournalRow row;
        row.id = query.value(0).toInt();
        row.number = query.value(1).toString();
        row.createdAt = query.value(2).toString();
        row.total = Money::fromMillimes(query.value(3).toLongLong());
        row.status = query.value(4).toString();
        row.userName = query.value(5).toString();
        row.method = query.value(6).toString();
        row.creditNoteNumber = query.value(7).toString();
        row.creditNoteCount = query.value(8).toInt();
        row.refundedTotal = Money::fromMillimes(query.value(9).toLongLong());
        rows.append(row);
    }
    return Result<QList<SaleJournalRow>>::ok(std::move(rows));
}

Result<Money> SqliteSaleRepository::recordCashClosure(Money countedCash,
                                                      int userId)
{
    // Théorique = encaissements espèces du jour (ventes complétées)
    // − remboursements espèces d'avoirs émis aujourd'hui.
    QSqlQuery expected(db());
    if (!expected.exec(QStringLiteral(
            "SELECT COALESCE((SELECT SUM(p.amount) FROM payments p "
            "  JOIN sales s ON s.id = p.sale_id "
            "  WHERE p.method = 'cash' AND s.status = 'completed' "
            "  AND date(p.created_at) = date('now')), 0) "
            "- COALESCE((SELECT SUM(cn.total) FROM credit_notes cn "
            "  WHERE cn.refund_method = 'cash' "
            "  AND date(cn.created_at) = date('now')), 0)"))
        || !expected.next())
        return Result<Money>::fail(QStringLiteral("closure.expected"),
                                   expected.lastError().text());
    const Money expectedCash = Money::fromMillimes(expected.value(0).toLongLong());
    const Money gap = countedCash - expectedCash;

    QSqlQuery insert(db());
    insert.prepare(QStringLiteral(
        "INSERT INTO cash_closures (uuid, expected_cash, counted_cash, gap, user_id) "
        "VALUES (:uuid, :expected, :counted, :gap, :user)"));
    insert.bindValue(QStringLiteral(":uuid"),
                     QUuid::createUuid().toString(QUuid::WithoutBraces));
    insert.bindValue(QStringLiteral(":expected"), expectedCash.millimes());
    insert.bindValue(QStringLiteral(":counted"), countedCash.millimes());
    insert.bindValue(QStringLiteral(":gap"), gap.millimes());
    insert.bindValue(QStringLiteral(":user"),
                     userId > 0 ? QVariant(userId) : QVariant());
    if (!insert.exec())
        return Result<Money>::fail(QStringLiteral("closure.insert"),
                                   insert.lastError().text());
    return Result<Money>::ok(gap);
}

Result<DayTotals> SqliteSaleRepository::todayTotals()
{
    DayTotals totals;

    QSqlQuery sales(db());
    if (!sales.exec(QStringLiteral(
            "SELECT count(*), COALESCE(SUM(total), 0) FROM sales "
            "WHERE date(created_at) = date('now') AND status = 'completed'")))
        return Result<DayTotals>::fail(QStringLiteral("sale.totals"),
                                       sales.lastError().text());
    if (sales.next()) {
        totals.saleCount = sales.value(0).toInt();
        totals.total = Money::fromMillimes(sales.value(1).toLongLong());
    }

    QSqlQuery payments(db());
    if (!payments.exec(QStringLiteral(
            "SELECT p.method, COALESCE(SUM(p.amount), 0) "
            "FROM payments p JOIN sales s ON s.id = p.sale_id "
            "WHERE date(s.created_at) = date('now') AND s.status = 'completed' "
            "GROUP BY p.method")))
        return Result<DayTotals>::fail(QStringLiteral("sale.totals"),
                                       payments.lastError().text());
    while (payments.next()) {
        const Money amount = Money::fromMillimes(payments.value(1).toLongLong());
        const QString method = payments.value(0).toString();
        if (method == QLatin1String("cash"))
            totals.cash = amount;
        else if (method == QLatin1String("cheque"))
            totals.cheque = amount;
        else if (method == QLatin1String("transfer"))
            totals.transfer = amount;
    }
    return Result<DayTotals>::ok(totals);
}

} // namespace nursera
