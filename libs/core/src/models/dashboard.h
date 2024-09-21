#pragma once

#include "common/money.h"

#include <QString>

namespace nursera {

// Indicateurs du tableau de bord (F10-01).
struct DashboardKpis
{
    int todaySaleCount = 0;
    Money todayTotal;
    Money weekTotal;  // 7 jours glissants
    Money monthTotal; // mois calendaire courant

    Money averageBasket() const
    {
        return todaySaleCount > 0
            ? Money::fromMillimes(todayTotal.millimes() / todaySaleCount)
            : Money{};
    }
};

struct TopProductRow
{
    QString label;   // snapshot de vente
    int qtySold = 0;
    Money revenue;
};

struct LowStockRow
{
    QString productFr;
    QString packaging;
    int qty = 0;
    int alertThreshold = -1;

    bool isNegative() const { return qty < 0; }
};

// Point d'une série temporelle (courbe/diagramme, F10-01/07).
struct TrendPoint
{
    QString label;      // "14/07" (jour) ou "juil." (mois)
    QString fullLabel;  // "14 juillet 2026" pour le tooltip
    Money total;
};

} // namespace nursera
