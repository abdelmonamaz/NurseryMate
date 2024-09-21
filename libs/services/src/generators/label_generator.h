#pragma once

#include "common/result.h"

#include <QList>
#include <QString>

namespace nursera {

// Étiquettes produit avec QR imprimables (F02-07). Planche A4, grille
// 3×8, QR encodant le SKU/code-barres (scan caisse) + nom + prix.
class LabelGenerator
{
public:
    struct Label
    {
        QString qrData;   // SKU ou code-barres
        QString nameFr;
        QString priceDisplay;
        QString sku;
    };

    // Génère <outputDir>/etiquettes-<horodatage>.pdf.
    static Result<QString> generatePdf(const QList<Label>& labels,
                                       const QString& outputDir);
};

} // namespace nursera
