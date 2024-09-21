#pragma once

#include "repositories/idashboard_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteDashboardRepository : public IDashboardRepository
{
public:
    explicit SqliteDashboardRepository(QString connectionName);

    Result<DashboardKpis> kpis() override;
    Result<QList<TopProductRow>> topProducts(int days = 30,
                                             int limit = 5) override;
    Result<QList<LowStockRow>> lowStock(int limit = 8) override;
    Result<QList<TrendPoint>> salesDaily(int days = 14) override;
    Result<QList<TrendPoint>> salesMonthly(int months = 6) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
