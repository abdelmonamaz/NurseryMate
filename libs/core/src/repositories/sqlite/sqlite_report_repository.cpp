#include "repositories/sqlite/sqlite_report_repository.h"

#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

// Bornes de dates incluses : date(created_at) BETWEEN from AND to.
void bindRange(QSqlQuery& query, const QString& from, const QString& to)
{
    query.bindValue(QStringLiteral(":from"), from);
    query.bindValue(QStringLiteral(":to"), to);
}

QString lossReasonLabel(const QString& code)
{
    if (code == QLatin1String("mortality")) return QStringLiteral("Mortalité");
    if (code == QLatin1String("disease")) return QStringLiteral("Maladie");
    if (code == QLatin1String("frost")) return QStringLiteral("Gel");
    if (code == QLatin1String("breakage")) return QStringLiteral("Casse");
    return QStringLiteral("Autre");
}

} // namespace

SqliteReportRepository::SqliteReportRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<SalesSummary> SqliteReportRepository::salesSummary(const QString& from,
                                                          const QString& to)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT count(*), COALESCE(SUM(total), 0) FROM sales "
        "WHERE status = 'completed' "
        "AND date(created_at) BETWEEN :from AND :to"));
    bindRange(query, from, to);
    if (!query.exec() || !query.next())
        return Result<SalesSummary>::fail(QStringLiteral("report.summary"),
                                          query.lastError().text());
    SalesSummary summary;
    summary.saleCount = query.value(0).toInt();
    summary.total = Money::fromMillimes(query.value(1).toLongLong());

    // CA net d'avoirs : les avoirs comptent à LEUR date d'émission (la
    // vente d'origine peut être antérieure à la période).
    QSqlQuery credits(db());
    credits.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(total), 0) FROM credit_notes "
        "WHERE date(created_at) BETWEEN :from AND :to"));
    bindRange(credits, from, to);
    if (!credits.exec() || !credits.next())
        return Result<SalesSummary>::fail(QStringLiteral("report.summary"),
                                          credits.lastError().text());
    summary.creditTotal = Money::fromMillimes(credits.value(0).toLongLong());
    return Result<SalesSummary>::ok(summary);
}

Result<QList<ReportRow>> SqliteReportRepository::salesByCategory(const QString& from,
                                                                 const QString& to)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT COALESCE(c.name_fr, 'Sans catégorie'), "
        "SUM(sl.qty), SUM(sl.line_total) "
        "FROM sale_lines sl "
        "JOIN sales s ON s.id = sl.sale_id AND s.status = 'completed' "
        "LEFT JOIN variants v ON v.id = sl.variant_id "
        "LEFT JOIN products p ON p.id = v.product_id "
        "LEFT JOIN categories c ON c.id = p.category_id "
        "WHERE date(s.created_at) BETWEEN :from AND :to "
        "GROUP BY c.id ORDER BY SUM(sl.line_total) DESC"));
    bindRange(query, from, to);
    if (!query.exec())
        return Result<QList<ReportRow>>::fail(QStringLiteral("report.category"),
                                              query.lastError().text());
    QList<ReportRow> rows;
    while (query.next()) {
        ReportRow row;
        row.label = query.value(0).toString();
        row.qty = query.value(1).toInt();
        row.amount = Money::fromMillimes(query.value(2).toLongLong());
        rows.append(row);
    }
    return Result<QList<ReportRow>>::ok(std::move(rows));
}

Result<QList<ReportRow>> SqliteReportRepository::salesByPayment(const QString& from,
                                                                const QString& to)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT p.method, COUNT(DISTINCT p.sale_id), SUM(p.amount) "
        "FROM payments p JOIN sales s ON s.id = p.sale_id AND s.status = 'completed' "
        "WHERE date(s.created_at) BETWEEN :from AND :to "
        "GROUP BY p.method ORDER BY SUM(p.amount) DESC"));
    bindRange(query, from, to);
    if (!query.exec())
        return Result<QList<ReportRow>>::fail(QStringLiteral("report.payment"),
                                              query.lastError().text());
    QList<ReportRow> rows;
    while (query.next()) {
        const QString method = query.value(0).toString();
        ReportRow row;
        row.label = method == QLatin1String("cash") ? QStringLiteral("Espèces")
            : method == QLatin1String("cheque") ? QStringLiteral("Chèque")
            : method == QLatin1String("transfer") ? QStringLiteral("Virement")
            : method;
        row.qty = query.value(1).toInt();
        row.amount = Money::fromMillimes(query.value(2).toLongLong());
        rows.append(row);
    }
    return Result<QList<ReportRow>>::ok(std::move(rows));
}

Result<QList<ReportRow>> SqliteReportRepository::stockValuation()
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT p.name_fr, v.packaging, SUM(st.qty), v.avg_cost "
            "FROM stock st "
            "JOIN variants v ON v.id = st.variant_id "
            "JOIN products p ON p.id = v.product_id "
            "GROUP BY v.id HAVING SUM(st.qty) > 0 "
            "ORDER BY SUM(st.qty) * v.avg_cost DESC")))
        return Result<QList<ReportRow>>::fail(QStringLiteral("report.stockVal"),
                                              query.lastError().text());
    QList<ReportRow> rows;
    while (query.next()) {
        ReportRow row;
        row.label = query.value(0).toString();
        row.sub = query.value(1).toString();
        row.qty = query.value(2).toInt();
        const qint64 unitCost = query.value(3).toLongLong();
        row.amount = Money::fromMillimes(row.qty * unitCost);
        rows.append(row);
    }
    return Result<QList<ReportRow>>::ok(std::move(rows));
}

Result<QList<MarginRow>> SqliteReportRepository::margins(const QString& from,
                                                         const QString& to,
                                                         bool byProduct)
{
    // Coût des lignes libres (variant NULL) : 0 — marge = CA (prestations).
    const QString groupLabel = byProduct
        ? QStringLiteral("COALESCE(p.name_fr, 'Prestations / lignes libres')")
        : QStringLiteral("COALESCE(c.name_fr, 'Sans catégorie')");
    const QString groupKey =
        byProduct ? QStringLiteral("p.id") : QStringLiteral("c.id");

    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT %1, SUM(sl.qty), SUM(sl.line_total), "
        "SUM(sl.qty * COALESCE(v.avg_cost, 0)) "
        "FROM sale_lines sl "
        "JOIN sales s ON s.id = sl.sale_id AND s.status = 'completed' "
        "LEFT JOIN variants v ON v.id = sl.variant_id "
        "LEFT JOIN products p ON p.id = v.product_id "
        "LEFT JOIN categories c ON c.id = p.category_id "
        "WHERE date(s.created_at) BETWEEN :from AND :to "
        "GROUP BY %2 "
        "ORDER BY SUM(sl.line_total) - SUM(sl.qty * COALESCE(v.avg_cost, 0)) "
        "DESC").arg(groupLabel, groupKey));
    bindRange(query, from, to);
    if (!query.exec())
        return Result<QList<MarginRow>>::fail(QStringLiteral("report.margin"),
                                              query.lastError().text());
    QList<MarginRow> rows;
    while (query.next()) {
        MarginRow row;
        row.label = query.value(0).toString();
        row.qty = query.value(1).toInt();
        row.revenue = Money::fromMillimes(query.value(2).toLongLong());
        row.cost = Money::fromMillimes(query.value(3).toLongLong());
        rows.append(row);
    }
    return Result<QList<MarginRow>>::ok(std::move(rows));
}

Result<QList<ReportRow>> SqliteReportRepository::productionLosses(const QString& from,
                                                                  const QString& to)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT e.loss_reason, SUM(e.qty) FROM batch_events e "
        "WHERE e.kind = 'loss' "
        "AND date(e.created_at) BETWEEN :from AND :to "
        "GROUP BY e.loss_reason ORDER BY SUM(e.qty) DESC"));
    bindRange(query, from, to);
    if (!query.exec())
        return Result<QList<ReportRow>>::fail(QStringLiteral("report.losses"),
                                              query.lastError().text());
    QList<ReportRow> rows;
    while (query.next()) {
        ReportRow row;
        row.label = lossReasonLabel(query.value(0).toString());
        row.qty = query.value(1).toInt();
        rows.append(row);
    }
    return Result<QList<ReportRow>>::ok(std::move(rows));
}

} // namespace nursera
