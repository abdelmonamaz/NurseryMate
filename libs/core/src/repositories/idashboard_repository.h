#pragma once

#include "common/result.h"
#include "models/dashboard.h"

#include <QList>

namespace nursera {

// Lectures agrégées du tableau de bord (F10-01) — lecture seule.
class IDashboardRepository
{
public:
    virtual ~IDashboardRepository() = default;

    virtual Result<DashboardKpis> kpis() = 0;
    virtual Result<QList<TopProductRow>> topProducts(int days = 30,
                                                     int limit = 5) = 0;
    // Variantes sous seuil ou négatives, pires d'abord (F03-05, RG-03.b).
    virtual Result<QList<LowStockRow>> lowStock(int limit = 8) = 0;

    // Séries temporelles du CA, trous comblés à zéro (F10-01/07).
    virtual Result<QList<TrendPoint>> salesDaily(int days = 14) = 0;
    virtual Result<QList<TrendPoint>> salesMonthly(int months = 6) = 0;
};

} // namespace nursera
