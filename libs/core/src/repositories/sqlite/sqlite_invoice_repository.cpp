#include "repositories/sqlite/sqlite_invoice_repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <map>
#include <utility>

namespace nursera {

SqliteInvoiceRepository::SqliteInvoiceRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<int> SqliteInvoiceRepository::invoiceIdForSale(int saleId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id FROM invoices WHERE sale_id = :sale ORDER BY id LIMIT 1"));
    query.bindValue(QStringLiteral(":sale"), saleId);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("invoice.forSale"),
                                 query.lastError().text());
    return Result<int>::ok(query.next() ? query.value(0).toInt() : 0);
}

Result<QString> SqliteInvoiceRepository::nextInvoiceNumber()
{
    const int year = QDate::currentDate().year();
    QSqlQuery upsert(db());
    upsert.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) "
        "VALUES ('invoice', :year, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    upsert.bindValue(QStringLiteral(":year"), year);
    if (!upsert.exec())
        return Result<QString>::fail(QStringLiteral("invoice.counter"),
                                     upsert.lastError().text());

    QSqlQuery read(db());
    read.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters "
        "WHERE kind = 'invoice' AND year = :year"));
    read.bindValue(QStringLiteral(":year"), year);
    if (!read.exec() || !read.next())
        return Result<QString>::fail(QStringLiteral("invoice.counter"),
                                     read.lastError().text());
    return Result<QString>::ok(QStringLiteral("F-%1-%2")
                                   .arg(year)
                                   .arg(read.value(0).toInt(), 5, 10,
                                        QLatin1Char('0')));
}

Result<Invoice> SqliteInvoiceRepository::createFromSale(int saleId,
                                                        Money stampDuty,
                                                        int userId)
{
    // Idempotent : une seule facture par vente
    if (const auto existing = invoiceIdForSale(saleId);
        existing.isOk() && existing.value() > 0) {
        QSqlQuery byId(db());
        byId.prepare(QStringLiteral(
            "SELECT id, uuid, number, sale_id, customer_id, "
            "COALESCE(customer_name, ''), COALESCE(customer_tax_id, ''), "
            "subtotal_ht, vat_total, stamp_duty, total, issued_at "
            "FROM invoices WHERE id = :id"));
        byId.bindValue(QStringLiteral(":id"), existing.value());
        if (byId.exec() && byId.next()) {
            Invoice invoice;
            invoice.id = byId.value(0).toInt();
            invoice.uuid = byId.value(1).toString();
            invoice.number = byId.value(2).toString();
            invoice.saleId = byId.value(3).toInt();
            invoice.customerId = byId.value(4).toInt();
            invoice.customerName = byId.value(5).toString();
            invoice.customerTaxId = byId.value(6).toString();
            invoice.subtotalHt = Money::fromMillimes(byId.value(7).toLongLong());
            invoice.vatTotal = Money::fromMillimes(byId.value(8).toLongLong());
            invoice.stampDuty = Money::fromMillimes(byId.value(9).toLongLong());
            invoice.total = Money::fromMillimes(byId.value(10).toLongLong());
            invoice.issuedAt = byId.value(11).toString();
            return Result<Invoice>::ok(std::move(invoice));
        }
    }

    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<Invoice>::fail(QStringLiteral("invoice.tx"),
                                     database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<Invoice>::fail(std::move(code), std::move(message));
    };

    QSqlQuery sale(database);
    sale.prepare(QStringLiteral(
        "SELECT s.status, s.vat_total, s.total, s.customer_id, "
        "COALESCE(c.name, ''), COALESCE(c.tax_id, '') "
        "FROM sales s LEFT JOIN customers c ON c.id = s.customer_id "
        "WHERE s.id = :id"));
    sale.bindValue(QStringLiteral(":id"), saleId);
    if (!sale.exec() || !sale.next())
        return fail(QStringLiteral("invoice.saleNotFound"),
                    QStringLiteral("Vente introuvable."));
    if (sale.value(0).toString() != QLatin1String("completed"))
        return fail(QStringLiteral("invoice.saleStatus"),
                    QStringLiteral("Vente annulée : facture impossible."));

    Invoice invoice;
    invoice.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    invoice.saleId = saleId;
    invoice.vatTotal = Money::fromMillimes(sale.value(1).toLongLong());
    const Money saleTtc = Money::fromMillimes(sale.value(2).toLongLong());
    invoice.subtotalHt = saleTtc - invoice.vatTotal;
    invoice.stampDuty = stampDuty;
    invoice.total = saleTtc + stampDuty;
    invoice.customerId = sale.value(3).toInt();
    invoice.customerName = sale.value(4).toString();
    invoice.customerTaxId = sale.value(5).toString();

    const auto number = nextInvoiceNumber();
    if (!number)
        return fail(number.error().code, number.error().message);
    invoice.number = number.value();

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO invoices (uuid, number, sale_id, customer_id, "
        "customer_name, customer_tax_id, subtotal_ht, vat_total, stamp_duty, "
        "total, user_id) "
        "VALUES (:uuid, :number, :sale, :customer, :name, :tax, :ht, :vat, "
        ":stamp, :total, :user)"));
    insert.bindValue(QStringLiteral(":uuid"), invoice.uuid);
    insert.bindValue(QStringLiteral(":number"), invoice.number);
    insert.bindValue(QStringLiteral(":sale"), saleId);
    insert.bindValue(QStringLiteral(":customer"),
                     invoice.customerId > 0 ? QVariant(invoice.customerId)
                                            : QVariant());
    insert.bindValue(QStringLiteral(":name"),
                     invoice.customerName.isEmpty() ? QVariant()
                                                    : QVariant(invoice.customerName));
    insert.bindValue(QStringLiteral(":tax"),
                     invoice.customerTaxId.isEmpty()
                         ? QVariant()
                         : QVariant(invoice.customerTaxId));
    insert.bindValue(QStringLiteral(":ht"), invoice.subtotalHt.millimes());
    insert.bindValue(QStringLiteral(":vat"), invoice.vatTotal.millimes());
    insert.bindValue(QStringLiteral(":stamp"), invoice.stampDuty.millimes());
    insert.bindValue(QStringLiteral(":total"), invoice.total.millimes());
    insert.bindValue(QStringLiteral(":user"),
                     userId > 0 ? QVariant(userId) : QVariant());
    if (!insert.exec())
        return fail(QStringLiteral("invoice.insert"), insert.lastError().text());
    invoice.id = insert.lastInsertId().toInt();

    if (!database.commit()) {
        database.rollback();
        return Result<Invoice>::fail(QStringLiteral("invoice.commit"),
                                     database.lastError().text());
    }
    return Result<Invoice>::ok(std::move(invoice));
}

Result<InvoiceDetails> SqliteInvoiceRepository::details(int invoiceId)
{
    InvoiceDetails out;

    QSqlQuery header(db());
    header.prepare(QStringLiteral(
        "SELECT i.uuid, i.number, i.sale_id, i.customer_id, "
        "COALESCE(i.customer_name, ''), COALESCE(i.customer_tax_id, ''), "
        "i.subtotal_ht, i.vat_total, i.stamp_duty, i.total, i.issued_at, "
        "s.number, s.subtotal, s.discount "
        "FROM invoices i JOIN sales s ON s.id = i.sale_id WHERE i.id = :id"));
    header.bindValue(QStringLiteral(":id"), invoiceId);
    if (!header.exec() || !header.next())
        return Result<InvoiceDetails>::fail(QStringLiteral("invoice.notFound"),
                                            QStringLiteral("Facture introuvable."));

    out.header.id = invoiceId;
    out.header.uuid = header.value(0).toString();
    out.header.number = header.value(1).toString();
    out.header.saleId = header.value(2).toInt();
    out.header.customerId = header.value(3).toInt();
    out.header.customerName = header.value(4).toString();
    out.header.customerTaxId = header.value(5).toString();
    out.header.subtotalHt = Money::fromMillimes(header.value(6).toLongLong());
    out.header.vatTotal = Money::fromMillimes(header.value(7).toLongLong());
    out.header.stampDuty = Money::fromMillimes(header.value(8).toLongLong());
    out.header.total = Money::fromMillimes(header.value(9).toLongLong());
    out.header.issuedAt = header.value(10).toString();
    out.saleNumber = header.value(11).toString();
    const Money saleSubtotal = Money::fromMillimes(header.value(12).toLongLong());
    out.globalDiscount = Money::fromMillimes(header.value(13).toLongLong());

    // Lignes de la vente (TTC), avec proratisation de la remise globale
    QSqlQuery lines(db());
    lines.prepare(QStringLiteral(
        "SELECT label_snapshot, qty, unit_price, vat_rate, line_total "
        "FROM sale_lines WHERE sale_id = :sale ORDER BY id"));
    lines.bindValue(QStringLiteral(":sale"), out.header.saleId);
    if (!lines.exec())
        return Result<InvoiceDetails>::fail(QStringLiteral("invoice.lines"),
                                            lines.lastError().text());

    struct RawLine
    {
        QString label;
        int qty;
        int rate;
        qint64 ttc;
    };
    QList<RawLine> raw;
    while (lines.next())
        raw.append({lines.value(0).toString(), lines.value(1).toInt(),
                    lines.value(3).toInt(), lines.value(4).toLongLong()});

    const qint64 discount = out.globalDiscount.millimes();
    const qint64 subtotal = qMax<qint64>(1, saleSubtotal.millimes());
    std::map<int, std::pair<qint64, qint64>> vatByRate; // rate -> {ht, vat}

    qint64 discountApplied = 0;
    for (int i = 0; i < raw.size(); ++i) {
        const RawLine& line = raw.at(i);
        // Part de remise proratisée (le reste sur la dernière ligne)
        const qint64 share = (i == raw.size() - 1)
            ? discount - discountApplied
            : discount * line.ttc / subtotal;
        discountApplied += share;
        const qint64 netTtc = line.ttc - share;

        const Money net = Money::fromMillimes(netTtc);
        const Money vat = net.vatFromTtc(line.rate);
        const Money ht = net - vat;

        SaleDetailLine detail;
        detail.label = line.label;
        detail.qty = line.qty;
        detail.unitPrice = line.qty > 0
            ? Money::fromMillimes(ht.millimes() / line.qty)
            : Money{};
        detail.lineTotal = ht;
        out.lines.append(detail);

        vatByRate[line.rate].first += ht.millimes();
        vatByRate[line.rate].second += vat.millimes();
    }

    for (const auto& [rate, amounts] : vatByRate) {
        VatBreakdownRow row;
        row.ratePercent = rate;
        row.baseHt = Money::fromMillimes(amounts.first);
        row.vat = Money::fromMillimes(amounts.second);
        out.vatRows.append(row);
    }

    return Result<InvoiceDetails>::ok(std::move(out));
}

Result<void> SqliteInvoiceRepository::setPdfPath(int invoiceId,
                                                 const QString& path)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE invoices SET pdf_path = :path WHERE id = :id"));
    query.bindValue(QStringLiteral(":path"), path);
    query.bindValue(QStringLiteral(":id"), invoiceId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("invoice.pdfPath"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
