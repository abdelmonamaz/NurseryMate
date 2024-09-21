#include "repositories/sqlite/sqlite_dashboard_repository.h"

#include <QDate>
#include <QLocale>
#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {

SqliteDashboardRepository::SqliteDashboardRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Result<DashboardKpis> SqliteDashboardRepository::kpis()
{
    DashboardKpis kpis;

    // CA net d'avoirs : chaque période déduit les avoirs émis pendant elle
    // (à LEUR date d'émission, la vente contre-passée pouvant être antérieure).
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT count(*), COALESCE(SUM(total), 0) "
            "- (SELECT COALESCE(SUM(total), 0) FROM credit_notes "
            "   WHERE date(created_at) = date('now')) "
            "FROM sales "
            "WHERE status = 'completed' AND date(created_at) = date('now')")))
        return Result<DashboardKpis>::fail(QStringLiteral("dashboard.today"),
                                           query.lastError().text());
    if (query.next()) {
        kpis.todaySaleCount = query.value(0).toInt();
        kpis.todayTotal = Money::fromMillimes(query.value(1).toLongLong());
    }

    if (!query.exec(QStringLiteral(
            "SELECT COALESCE(SUM(total), 0) "
            "- (SELECT COALESCE(SUM(total), 0) FROM credit_notes "
            "   WHERE date(created_at) >= date('now', '-6 days')) "
            "FROM sales "
            "WHERE status = 'completed' "
            "AND date(created_at) >= date('now', '-6 days')")))
        return Result<DashboardKpis>::fail(QStringLiteral("dashboard.week"),
                                           query.lastError().text());
    if (query.next())
        kpis.weekTotal = Money::fromMillimes(query.value(0).toLongLong());

    if (!query.exec(QStringLiteral(
            "SELECT COALESCE(SUM(total), 0) "
            "- (SELECT COALESCE(SUM(total), 0) FROM credit_notes "
            "   WHERE strftime('%Y-%m', created_at) = strftime('%Y-%m', 'now')) "
            "FROM sales "
            "WHERE status = 'completed' "
            "AND strftime('%Y-%m', created_at) = strftime('%Y-%m', 'now')")))
        return Result<DashboardKpis>::fail(QStringLiteral("dashboard.month"),
                                           query.lastError().text());
    if (query.next())
        kpis.monthTotal = Money::fromMillimes(query.value(0).toLongLong());

    return Result<DashboardKpis>::ok(kpis);
}

Result<QList<TopProductRow>> SqliteDashboardRepository::topProducts(int days,
                                                                    int limit)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT sl.label_snapshot, SUM(sl.qty), SUM(sl.line_total) "
        "FROM sale_lines sl JOIN sales s ON s.id = sl.sale_id "
        "WHERE s.status = 'completed' "
        "AND s.created_at >= datetime('now', '-' || :days || ' days') "
        "GROUP BY sl.label_snapshot "
        "ORDER BY SUM(sl.line_total) DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":days"), days);
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<TopProductRow>>::fail(QStringLiteral("dashboard.top"),
                                                  query.lastError().text());

    QList<TopProductRow> rows;
    while (query.next()) {
        TopProductRow row;
        row.label = query.value(0).toString();
        row.qtySold = query.value(1).toInt();
        row.revenue = Money::fromMillimes(query.value(2).toLongLong());
        rows.append(row);
    }
    return Result<QList<TopProductRow>>::ok(std::move(rows));
}

Result<QList<TrendPoint>> SqliteDashboardRepository::salesDaily(int days)
{
    // Totaux par jour existants
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT date(created_at), COALESCE(SUM(total), 0) FROM sales "
        "WHERE status = 'completed' "
        "AND date(created_at) >= date('now', :since) "
        "GROUP BY date(created_at)"));
    query.bindValue(QStringLiteral(":since"),
                    QStringLiteral("-%1 days").arg(days - 1));
    if (!query.exec())
        return Result<QList<TrendPoint>>::fail(QStringLiteral("dashboard.daily"),
                                               query.lastError().text());
    QHash<QString, qint64> byDay;
    while (query.next())
        byDay.insert(query.value(0).toString(), query.value(1).toLongLong());

    // Avoirs déduits au jour de leur émission (CA net)
    QSqlQuery credits(db());
    credits.prepare(QStringLiteral(
        "SELECT date(created_at), COALESCE(SUM(total), 0) FROM credit_notes "
        "WHERE date(created_at) >= date('now', :since) "
        "GROUP BY date(created_at)"));
    credits.bindValue(QStringLiteral(":since"),
                      QStringLiteral("-%1 days").arg(days - 1));
    if (!credits.exec())
        return Result<QList<TrendPoint>>::fail(QStringLiteral("dashboard.daily"),
                                               credits.lastError().text());
    while (credits.next())
        byDay[credits.value(0).toString()] -= credits.value(1).toLongLong();

    // Comble les jours sans vente à zéro, ordre chronologique
    const QLocale locale;
    QList<TrendPoint> points;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate date = today.addDays(-i);
        TrendPoint point;
        point.label = date.toString(QStringLiteral("dd/MM"));
        point.fullLabel = locale.toString(date, QLocale::LongFormat);
        point.total = Money::fromMillimes(
            byDay.value(date.toString(Qt::ISODate), 0));
        points.append(point);
    }
    return Result<QList<TrendPoint>>::ok(std::move(points));
}

Result<QList<TrendPoint>> SqliteDashboardRepository::salesMonthly(int months)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT strftime('%Y-%m', created_at), COALESCE(SUM(total), 0) FROM sales "
        "WHERE status = 'completed' "
        "AND created_at >= date('now', :since, 'start of month') "
        "GROUP BY strftime('%Y-%m', created_at)"));
    query.bindValue(QStringLiteral(":since"),
                    QStringLiteral("-%1 months").arg(months - 1));
    if (!query.exec())
        return Result<QList<TrendPoint>>::fail(QStringLiteral("dashboard.monthly"),
                                               query.lastError().text());
    QHash<QString, qint64> byMonth;
    while (query.next())
        byMonth.insert(query.value(0).toString(), query.value(1).toLongLong());

    // Avoirs déduits au mois de leur émission (CA net)
    QSqlQuery credits(db());
    credits.prepare(QStringLiteral(
        "SELECT strftime('%Y-%m', created_at), COALESCE(SUM(total), 0) "
        "FROM credit_notes "
        "WHERE created_at >= date('now', :since, 'start of month') "
        "GROUP BY strftime('%Y-%m', created_at)"));
    credits.bindValue(QStringLiteral(":since"),
                      QStringLiteral("-%1 months").arg(months - 1));
    if (!credits.exec())
        return Result<QList<TrendPoint>>::fail(QStringLiteral("dashboard.monthly"),
                                               credits.lastError().text());
    while (credits.next())
        byMonth[credits.value(0).toString()] -= credits.value(1).toLongLong();

    const QLocale locale;
    QList<TrendPoint> points;
    QDate cursor = QDate::currentDate();
    cursor.setDate(cursor.year(), cursor.month(), 1);
    for (int i = months - 1; i >= 0; --i) {
        const QDate month = cursor.addMonths(-i);
        TrendPoint point;
        point.label = locale.toString(month, QStringLiteral("MMM"));
        point.fullLabel = locale.toString(month, QStringLiteral("MMMM yyyy"));
        point.total = Money::fromMillimes(
            byMonth.value(month.toString(QStringLiteral("yyyy-MM")), 0));
        points.append(point);
    }
    return Result<QList<TrendPoint>>::ok(std::move(points));
}

Result<QList<LowStockRow>> SqliteDashboardRepository::lowStock(int limit)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT p.name_fr, v.packaging, COALESCE(SUM(st.qty), 0) AS qty, "
        "       v.alert_threshold "
        "FROM variants v "
        "JOIN products p ON p.id = v.product_id "
        "LEFT JOIN stock st ON st.variant_id = v.id "
        "WHERE v.active = 1 AND p.active = 1 "
        "GROUP BY v.id "
        "HAVING qty < 0 OR (v.alert_threshold IS NOT NULL "
        "                   AND qty <= v.alert_threshold) "
        "ORDER BY qty ASC LIMIT :limit"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec())
        return Result<QList<LowStockRow>>::fail(QStringLiteral("dashboard.lowStock"),
                                                query.lastError().text());

    QList<LowStockRow> rows;
    while (query.next()) {
        LowStockRow row;
        row.productFr = query.value(0).toString();
        row.packaging = query.value(1).toString();
        row.qty = query.value(2).toInt();
        row.alertThreshold = query.value(3).isNull() ? -1 : query.value(3).toInt();
        rows.append(row);
    }
    return Result<QList<LowStockRow>>::ok(std::move(rows));
}

} // namespace nursera
