#pragma once

#include "common/money.h"

#include <QString>

namespace nursera {

// Ligne générique d'un rapport tabulaire (F10-02..06).
struct ReportRow
{
    QString label;   // catégorie / produit / motif / mode de paiement
    QString sub;     // info secondaire (conditionnement, source…)
    int qty = 0;
    Money amount;    // CA, valeur de stock, valeur de perte…
};

struct SalesSummary
{
    int saleCount = 0;
    Money total;       // CA brut (ventes completed)
    Money creditTotal; // avoirs émis sur la période (par date d'avoir)
    Money net() const { return total - creditTotal; }
    Money averageBasket() const
    {
        return saleCount > 0
            ? Money::fromMillimes(total.millimes() / saleCount)
            : Money{};
    }
};

// Ligne du rapport de marge (F10-05) : CA − coût au CMP courant.
// NB : le CMP est celui d'AUJOURD'HUI (pas celui du jour de la vente) —
// approximation standard des petits POS, documentée.
struct MarginRow
{
    QString label;   // catégorie ou produit
    int qty = 0;
    Money revenue;   // CA (lignes des ventes complétées)
    Money cost;      // qty × avg_cost
    Money margin() const { return revenue - cost; }
    // Taux de marge sur CA, en pour-mille (ex. 425 = 42,5 %).
    int marginPerMille() const
    {
        return revenue.millimes() > 0
            ? static_cast<int>(margin().millimes() * 1000 / revenue.millimes())
            : 0;
    }
};

} // namespace nursera
