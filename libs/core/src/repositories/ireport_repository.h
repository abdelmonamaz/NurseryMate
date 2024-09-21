#pragma once

#include "common/result.h"
#include "models/report.h"

#include <QList>
#include <QString>

namespace nursera {

// Rapports détaillés avec plage de dates (M10, F10-02..06). Lecture seule.
// Les dates sont au format ISO "yyyy-MM-dd", bornes incluses.
class IReportRepository
{
public:
    virtual ~IReportRepository() = default;

    virtual Result<SalesSummary> salesSummary(const QString& from,
                                              const QString& to) = 0;
    // CA par catégorie de produit sur la période (F10-02).
    virtual Result<QList<ReportRow>> salesByCategory(const QString& from,
                                                     const QString& to) = 0;
    // CA par mode de paiement.
    virtual Result<QList<ReportRow>> salesByPayment(const QString& from,
                                                    const QString& to) = 0;

    // Valorisation du stock au coût moyen pondéré (F10-03), par variante
    // à quantité positive, plus la valeur totale en tête.
    virtual Result<QList<ReportRow>> stockValuation() = 0;

    // Pertes de production par motif sur la période (F10-04) — quantités.
    virtual Result<QList<ReportRow>> productionLosses(const QString& from,
                                                      const QString& to) = 0;

    // Rapport de marge (F10-05) : CA − coût CMP, groupé par catégorie
    // (byProduct=false) ou par produit, trié par marge décroissante.
    virtual Result<QList<MarginRow>> margins(const QString& from,
                                             const QString& to,
                                             bool byProduct) = 0;
};

} // namespace nursera
