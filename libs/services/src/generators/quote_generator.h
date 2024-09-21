#pragma once

#include "common/result.h"
#include "models/quote.h"
#include "repositories/isettings_repository.h"

#include <QString>

namespace nursera {

// Devis PDF A4 (F05-01) : en-tête société, client, lignes (produits ou
// prestations libres), remise, total, validité, montant en lettres.
class QuoteGenerator
{
public:
    struct CompanyInfo
    {
        QString name;
        QString tagline;
        QString address;
        QString phone;
        QString taxId;
    };

    static CompanyInfo companyFrom(ISettingsRepository& settings);
    static Result<QString> generatePdf(const QuoteDetails& quote,
                                       const CompanyInfo& company,
                                       const QString& outputDir);
};

} // namespace nursera
