#include "repositories/sqlite/sqlite_customer_repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

QString kindToString(CustomerKind kind)
{
    return kind == CustomerKind::Professional ? QStringLiteral("professional")
                                              : QStringLiteral("individual");
}

CustomerKind kindFromString(const QString& text)
{
    return text == QLatin1String("professional") ? CustomerKind::Professional
                                                 : CustomerKind::Individual;
}

QString methodToString(PaymentMethod method)
{
    switch (method) {
    case PaymentMethod::Cheque: return QStringLiteral("cheque");
    case PaymentMethod::Transfer: return QStringLiteral("transfer");
    case PaymentMethod::Cash: break;
    }
    return QStringLiteral("cash");
}

void bindCustomer(QSqlQuery& query, const Customer& customer)
{
    query.bindValue(QStringLiteral(":kind"), kindToString(customer.kind));
    query.bindValue(QStringLiteral(":name"), customer.name);
    query.bindValue(QStringLiteral(":phone"),
                    customer.phone.isEmpty() ? QVariant() : QVariant(customer.phone));
    query.bindValue(QStringLiteral(":phone2"),
                    customer.phone2.isEmpty() ? QVariant() : QVariant(customer.phone2));
    query.bindValue(QStringLiteral(":email"),
                    customer.email.isEmpty() ? QVariant() : QVariant(customer.email));
    query.bindValue(QStringLiteral(":address"),
                    customer.address.isEmpty() ? QVariant() : QVariant(customer.address));
    query.bindValue(QStringLiteral(":tax_id"),
                    customer.taxId.isEmpty() ? QVariant() : QVariant(customer.taxId));
    query.bindValue(QStringLiteral(":lang"), customer.lang);
    query.bindValue(QStringLiteral(":credit_limit"),
                    customer.creditLimitMillimes >= 0
                        ? QVariant(customer.creditLimitMillimes)
                        : QVariant());
    query.bindValue(QStringLiteral(":notes"),
                    customer.notes.isEmpty() ? QVariant() : QVariant(customer.notes));
    query.bindValue(QStringLiteral(":active"), customer.active ? 1 : 0);
}

// Encours en sous-requêtes corrélées (réutilisé dans search).
// Les paiements d'une vente annulée sont neutralisés (F04-08).
// Un avoir imputé sur l'encours (refund_method='credit') réduit la dette.
const QString kBalanceExpr = QStringLiteral(
    "COALESCE((SELECT SUM(total) FROM sales s "
    "          WHERE s.customer_id = c.id AND s.status = 'completed'), 0) "
    "- COALESCE((SELECT SUM(amount) FROM payments p "
    "            WHERE p.customer_id = c.id "
    "            AND (p.sale_id IS NULL OR (SELECT status FROM sales sx "
    "                 WHERE sx.id = p.sale_id) = 'completed')), 0) "
    "- COALESCE((SELECT SUM(cn.total) FROM credit_notes cn "
    "            JOIN sales sc ON sc.id = cn.sale_id "
    "            WHERE sc.customer_id = c.id "
    "            AND cn.refund_method = 'credit'), 0)");

} // namespace

SqliteCustomerRepository::SqliteCustomerRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<QList<CustomerRow>> SqliteCustomerRepository::search(const QString& term,
                                                            bool includeInactive)
{
    QString sql = QStringLiteral(
                      "SELECT c.id, c.kind, c.name, COALESCE(c.phone, ''), "
                      "c.credit_limit, %1, c.active FROM customers c ")
                      .arg(kBalanceExpr);
    QStringList where;
    const QString trimmedTerm = term.trimmed();
    if (!includeInactive)
        where << QStringLiteral("c.active = 1");
    if (!trimmedTerm.isEmpty())
        where << QStringLiteral("(c.name LIKE :t1 OR c.phone LIKE :t2)");
    if (!where.isEmpty())
        sql += QStringLiteral("WHERE ") + where.join(QStringLiteral(" AND ")) + QLatin1Char(' ');
    sql += QStringLiteral("ORDER BY c.name COLLATE NOCASE");

    QSqlQuery query(db());
    if (!query.prepare(sql))
        return Result<QList<CustomerRow>>::fail(QStringLiteral("customer.search"),
                                                query.lastError().text());
    if (!trimmedTerm.isEmpty()) {
        const QString like = QLatin1Char('%') + trimmedTerm + QLatin1Char('%');
        query.bindValue(QStringLiteral(":t1"), like);
        query.bindValue(QStringLiteral(":t2"), like);
    }
    if (!query.exec())
        return Result<QList<CustomerRow>>::fail(QStringLiteral("customer.search"),
                                                query.lastError().text());

    QList<CustomerRow> rows;
    while (query.next()) {
        CustomerRow row;
        row.id = query.value(0).toInt();
        row.kind = kindFromString(query.value(1).toString());
        row.name = query.value(2).toString();
        row.phone = query.value(3).toString();
        row.creditLimitMillimes =
            query.value(4).isNull() ? -1 : query.value(4).toLongLong();
        row.balance = Money::fromMillimes(query.value(5).toLongLong());
        row.active = query.value(6).toBool();
        rows.append(row);
    }
    return Result<QList<CustomerRow>>::ok(std::move(rows));
}

Result<Customer> SqliteCustomerRepository::byId(int id)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT id, kind, name, COALESCE(phone, ''), COALESCE(phone2, ''), "
        "COALESCE(email, ''), COALESCE(address, ''), COALESCE(tax_id, ''), "
        "lang, credit_limit, COALESCE(notes, ''), active "
        "FROM customers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<Customer>::fail(QStringLiteral("customer.byId"),
                                      query.lastError().text());
    if (!query.next())
        return Result<Customer>::fail(QStringLiteral("customer.notFound"),
                                      QStringLiteral("Client introuvable."));

    Customer customer;
    customer.id = query.value(0).toInt();
    customer.kind = kindFromString(query.value(1).toString());
    customer.name = query.value(2).toString();
    customer.phone = query.value(3).toString();
    customer.phone2 = query.value(4).toString();
    customer.email = query.value(5).toString();
    customer.address = query.value(6).toString();
    customer.taxId = query.value(7).toString();
    customer.lang = query.value(8).toString();
    customer.creditLimitMillimes =
        query.value(9).isNull() ? -1 : query.value(9).toLongLong();
    customer.notes = query.value(10).toString();
    customer.active = query.value(11).toBool();
    return Result<Customer>::ok(std::move(customer));
}

Result<int> SqliteCustomerRepository::insert(const Customer& customer)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO customers (kind, name, phone, phone2, email, address, "
        "tax_id, lang, credit_limit, notes, active) "
        "VALUES (:kind, :name, :phone, :phone2, :email, :address, "
        ":tax_id, :lang, :credit_limit, :notes, :active)"));
    bindCustomer(query, customer);
    if (!query.exec())
        return Result<int>::fail(QStringLiteral("customer.insert"),
                                 query.lastError().text());
    return Result<int>::ok(query.lastInsertId().toInt());
}

Result<void> SqliteCustomerRepository::update(const Customer& customer)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE customers SET kind = :kind, name = :name, phone = :phone, "
        "phone2 = :phone2, email = :email, address = :address, "
        "tax_id = :tax_id, lang = :lang, credit_limit = :credit_limit, "
        "notes = :notes, active = :active WHERE id = :id"));
    bindCustomer(query, customer);
    query.bindValue(QStringLiteral(":id"), customer.id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("customer.update"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<void> SqliteCustomerRepository::setActive(int id, bool active)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "UPDATE customers SET active = :active WHERE id = :id"));
    query.bindValue(QStringLiteral(":active"), active ? 1 : 0);
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("customer.setActive"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<bool> SqliteCustomerRepository::isReferenced(int id)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT EXISTS(SELECT 1 FROM sales WHERE customer_id = :c1) "
        "OR EXISTS(SELECT 1 FROM payments WHERE customer_id = :c2) "
        "OR EXISTS(SELECT 1 FROM quotes WHERE customer_id = :c3)"));
    query.bindValue(QStringLiteral(":c1"), id);
    query.bindValue(QStringLiteral(":c2"), id);
    query.bindValue(QStringLiteral(":c3"), id);
    if (!query.exec() || !query.next())
        return Result<bool>::fail(QStringLiteral("customer.referenced"),
                                  query.lastError().text());
    return Result<bool>::ok(query.value(0).toBool());
}

Result<void> SqliteCustomerRepository::remove(int id)
{
    // Garde-fou : jamais de suppression d'une fiche référencée (norme).
    if (const auto referenced = isReferenced(id); !referenced)
        return Result<void>::fail(referenced.error());
    else if (referenced.value())
        return Result<void>::fail(
            QStringLiteral("customer.referenced"),
            QStringLiteral("Ce client a des transactions — désactivez-le."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM customers WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), id);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("customer.remove"),
                                  query.lastError().text());
    return Result<void>::ok();
}

Result<bool> SqliteCustomerRepository::phoneExists(const QString& phone)
{
    if (phone.trimmed().isEmpty())
        return Result<bool>::ok(false);
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT 1 FROM customers WHERE phone = :phone AND active = 1 LIMIT 1"));
    query.bindValue(QStringLiteral(":phone"), phone.trimmed());
    if (!query.exec())
        return Result<bool>::fail(QStringLiteral("customer.phone"),
                                  query.lastError().text());
    return Result<bool>::ok(query.next());
}

Result<Money> SqliteCustomerRepository::balanceOf(int customerId)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT %1 FROM customers c WHERE c.id = :id")
                      .arg(kBalanceExpr));
    query.bindValue(QStringLiteral(":id"), customerId);
    if (!query.exec() || !query.next())
        return Result<Money>::fail(QStringLiteral("customer.balance"),
                                   query.lastError().text());
    return Result<Money>::ok(Money::fromMillimes(query.value(0).toLongLong()));
}

Result<void> SqliteCustomerRepository::recordPayment(int customerId, Money amount,
                                                     PaymentMethod method,
                                                     const QString& chequeNumber,
                                                     const QString& chequeBank,
                                                     int userId)
{
    if (customerId <= 0 || amount.millimes() <= 0)
        return Result<void>::fail(QStringLiteral("customer.payment"),
                                  QStringLiteral("Montant ou client invalide."));

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO payments (uuid, customer_id, method, amount, "
        "cheque_number, cheque_bank, user_id) "
        "VALUES (:uuid, :customer, :method, :amount, :cheque_number, "
        ":cheque_bank, :user)"));
    query.bindValue(QStringLiteral(":uuid"),
                    QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(QStringLiteral(":customer"), customerId);
    query.bindValue(QStringLiteral(":method"), methodToString(method));
    query.bindValue(QStringLiteral(":amount"), amount.millimes());
    query.bindValue(QStringLiteral(":cheque_number"),
                    chequeNumber.isEmpty() ? QVariant() : QVariant(chequeNumber));
    query.bindValue(QStringLiteral(":cheque_bank"),
                    chequeBank.isEmpty() ? QVariant() : QVariant(chequeBank));
    query.bindValue(QStringLiteral(":user"),
                    userId > 0 ? QVariant(userId) : QVariant());
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("customer.payment"),
                                  query.lastError().text());
    return Result<void>::ok();
}

} // namespace nursera
