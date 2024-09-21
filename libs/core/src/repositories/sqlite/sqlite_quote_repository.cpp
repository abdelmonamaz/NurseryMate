#include "repositories/sqlite/sqlite_quote_repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

QString statusToString(QuoteStatus status)
{
    switch (status) {
    case QuoteStatus::Sent: return QStringLiteral("sent");
    case QuoteStatus::Accepted: return QStringLiteral("accepted");
    case QuoteStatus::Refused: return QStringLiteral("refused");
    case QuoteStatus::Expired: return QStringLiteral("expired");
    case QuoteStatus::Draft: break;
    }
    return QStringLiteral("draft");
}

QuoteStatus statusFromString(const QString& text)
{
    if (text == QLatin1String("sent")) return QuoteStatus::Sent;
    if (text == QLatin1String("accepted")) return QuoteStatus::Accepted;
    if (text == QLatin1String("refused")) return QuoteStatus::Refused;
    if (text == QLatin1String("expired")) return QuoteStatus::Expired;
    return QuoteStatus::Draft;
}

} // namespace

SqliteQuoteRepository::SqliteQuoteRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<Quote> SqliteQuoteRepository::create(const QuoteDraft& draft)
{
    if (draft.lines.isEmpty())
        return Result<Quote>::fail(QStringLiteral("quote.empty"),
                                   QStringLiteral("Le devis est vide."));

    const int year = QDate::currentDate().year();
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<Quote>::fail(QStringLiteral("quote.tx"),
                                   database.lastError().text());
    const auto fail = [&database](QString code, QString message) {
        database.rollback();
        return Result<Quote>::fail(std::move(code), std::move(message));
    };

    QSqlQuery counter(database);
    counter.prepare(QStringLiteral(
        "INSERT INTO doc_counters (kind, year, next_number) VALUES ('quote', :y, 2) "
        "ON CONFLICT (kind, year) DO UPDATE SET next_number = next_number + 1"));
    counter.bindValue(QStringLiteral(":y"), year);
    if (!counter.exec())
        return fail(QStringLiteral("quote.counter"), counter.lastError().text());
    QSqlQuery readCounter(database);
    readCounter.prepare(QStringLiteral(
        "SELECT next_number - 1 FROM doc_counters WHERE kind = 'quote' AND year = :y"));
    readCounter.bindValue(QStringLiteral(":y"), year);
    if (!readCounter.exec() || !readCounter.next())
        return fail(QStringLiteral("quote.counter"), readCounter.lastError().text());

    Quote quote;
    quote.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    quote.number = QStringLiteral("D-%1-%2")
                       .arg(year)
                       .arg(readCounter.value(0).toInt(), 5, 10, QLatin1Char('0'));
    quote.total = draft.total();

    const QString validUntil = draft.validUntil.isEmpty()
        ? QDate::currentDate().addDays(30).toString(Qt::ISODate)
        : draft.validUntil;

    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO quotes (uuid, number, customer_id, customer_name, subtotal, "
        "discount, total, valid_until, note, user_id) "
        "VALUES (:uuid, :number, :customer, :name, :subtotal, :discount, :total, "
        ":valid, :note, :user)"));
    insert.bindValue(QStringLiteral(":uuid"), quote.uuid);
    insert.bindValue(QStringLiteral(":number"), quote.number);
    insert.bindValue(QStringLiteral(":customer"),
                     draft.customerId > 0 ? QVariant(draft.customerId) : QVariant());
    insert.bindValue(QStringLiteral(":name"),
                     draft.customerName.isEmpty() ? QVariant()
                                                  : QVariant(draft.customerName));
    insert.bindValue(QStringLiteral(":subtotal"), draft.subtotal().millimes());
    insert.bindValue(QStringLiteral(":discount"), draft.discount.millimes());
    insert.bindValue(QStringLiteral(":total"), quote.total.millimes());
    insert.bindValue(QStringLiteral(":valid"), validUntil);
    insert.bindValue(QStringLiteral(":note"),
                     draft.note.isEmpty() ? QVariant() : QVariant(draft.note));
    insert.bindValue(QStringLiteral(":user"),
                     draft.userId > 0 ? QVariant(draft.userId) : QVariant());
    if (!insert.exec())
        return fail(QStringLiteral("quote.insert"), insert.lastError().text());
    quote.id = insert.lastInsertId().toInt();

    for (const QuoteLine& line : draft.lines) {
        QSqlQuery insertLine(database);
        insertLine.prepare(QStringLiteral(
            "INSERT INTO quote_lines (quote_id, variant_id, label, qty, "
            "unit_price, line_total) "
            "VALUES (:quote, :variant, :label, :qty, :price, :total)"));
        insertLine.bindValue(QStringLiteral(":quote"), quote.id);
        insertLine.bindValue(QStringLiteral(":variant"),
                             line.variantId > 0 ? QVariant(line.variantId) : QVariant());
        insertLine.bindValue(QStringLiteral(":label"), line.label);
        insertLine.bindValue(QStringLiteral(":qty"), line.qty);
        insertLine.bindValue(QStringLiteral(":price"), line.unitPrice.millimes());
        insertLine.bindValue(QStringLiteral(":total"), line.lineTotal().millimes());
        if (!insertLine.exec())
            return fail(QStringLiteral("quote.line"), insertLine.lastError().text());
    }

    if (!database.commit()) {
        database.rollback();
        return Result<Quote>::fail(QStringLiteral("quote.commit"),
                                   database.lastError().text());
    }
    return Result<Quote>::ok(std::move(quote));
}

Result<QList<QuoteRow>> SqliteQuoteRepository::list(int limit)
{
    // Expiration automatique : un devis brouillon/envoyé dont la validité
    // est dépassée passe à 'expired' (accepté/refusé restent figés).
    QSqlQuery expire(db());
    expire.exec(QStringLiteral(
        "UPDATE quotes SET status = 'expired' "
        "WHERE status IN ('draft', 'sent') "
        "AND valid_until IS NOT NULL AND valid_until < date('now')"));

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT q.id, q.number, "
        "COALESCE(q.customer_name, COALESCE(c.name, '')), q.status, q.total, "
        "q.created_at, COALESCE(q.valid_until, '') "
        "FROM quotes q LEFT JOIN customers c ON c.id = q.customer_id "
        "ORDER BY q.id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<QuoteRow>>::fail(QStringLiteral("quote.list"),
                                             query.lastError().text());

    QList<QuoteRow> rows;
    while (query.next()) {
        QuoteRow row;
        row.id = query.value(0).toInt();
        row.number = query.value(1).toString();
        row.customerName = query.value(2).toString();
        row.status = statusFromString(query.value(3).toString());
        row.total = Money::fromMillimes(query.value(4).toLongLong());
        row.createdAt = query.value(5).toString();
        row.validUntil = query.value(6).toString();
        rows.append(row);
    }
    return Result<QList<QuoteRow>>::ok(std::move(rows));
}

Result<QuoteDetails> SqliteQuoteRepository::details(int quoteId)
{
    QSqlQuery header(db());
    header.prepare(QStringLiteral(
        "SELECT q.number, COALESCE(q.customer_name, COALESCE(c.name, '')), "
        "COALESCE(c.tax_id, ''), q.status, q.subtotal, q.discount, q.total, "
        "q.created_at, COALESCE(q.valid_until, ''), COALESCE(q.note, ''), "
        "COALESCE(q.customer_id, 0) "
        "FROM quotes q LEFT JOIN customers c ON c.id = q.customer_id "
        "WHERE q.id = :id"));
    header.bindValue(QStringLiteral(":id"), quoteId);
    if (!header.exec() || !header.next())
        return Result<QuoteDetails>::fail(QStringLiteral("quote.notFound"),
                                          QStringLiteral("Devis introuvable."));

    QuoteDetails details;
    details.id = quoteId;
    details.number = header.value(0).toString();
    details.customerName = header.value(1).toString();
    details.customerTaxId = header.value(2).toString();
    details.status = statusFromString(header.value(3).toString());
    details.subtotal = Money::fromMillimes(header.value(4).toLongLong());
    details.discount = Money::fromMillimes(header.value(5).toLongLong());
    details.total = Money::fromMillimes(header.value(6).toLongLong());
    details.createdAt = header.value(7).toString();
    details.validUntil = header.value(8).toString();
    details.note = header.value(9).toString();
    details.customerId = header.value(10).toInt();

    QSqlQuery lines(db());
    lines.prepare(QStringLiteral(
        "SELECT label, qty, unit_price, COALESCE(variant_id, 0) "
        "FROM quote_lines WHERE quote_id = :id ORDER BY id"));
    lines.bindValue(QStringLiteral(":id"), quoteId);
    if (!lines.exec())
        return Result<QuoteDetails>::fail(QStringLiteral("quote.lines"),
                                          lines.lastError().text());
    while (lines.next()) {
        QuoteLine line;
        line.label = lines.value(0).toString();
        line.qty = lines.value(1).toInt();
        line.unitPrice = Money::fromMillimes(lines.value(2).toLongLong());
        line.variantId = lines.value(3).toInt();
        details.lines.append(line);
    }
    return Result<QuoteDetails>::ok(std::move(details));
}

Result<void> SqliteQuoteRepository::setStatus(int quoteId, QuoteStatus status)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE quotes SET status = :s WHERE id = :id"));
    query.bindValue(QStringLiteral(":s"), statusToString(status));
    query.bindValue(QStringLiteral(":id"), quoteId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("quote.status"),
                                  query.lastError().text());

    // Réouverture (retour à Envoyé) d'un devis à validité dépassée :
    // prolonge de 30 j, sinon il re-expirerait au prochain listage.
    if (status == QuoteStatus::Sent) {
        QSqlQuery extend(db());
        extend.prepare(QStringLiteral(
            "UPDATE quotes SET valid_until = date('now', '+30 day') "
            "WHERE id = :id AND valid_until IS NOT NULL "
            "AND valid_until < date('now')"));
        extend.bindValue(QStringLiteral(":id"), quoteId);
        extend.exec();
    }
    return Result<void>::ok();
}

Result<void> SqliteQuoteRepository::setPdfPath(int quoteId, const QString& path)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE quotes SET pdf_path = :p WHERE id = :id"));
    query.bindValue(QStringLiteral(":p"), path);
    query.bindValue(QStringLiteral(":id"), quoteId);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("quote.pdfPath"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
