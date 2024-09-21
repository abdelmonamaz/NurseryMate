#pragma once

#include "common/result.h"
#include "models/invoice.h"
#include "repositories/isettings_repository.h"

#include <QString>

namespace nursera {

// Facture PDF A4 conforme Tunisie (F05-03) : en-tête société + matricule,
// client + matricule, tableau HT, récap TVA par taux, timbre fiscal,
// total TTC en chiffres et en lettres (montant en lettres, doc 02 §6).
class InvoiceGenerator
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

    static Result<QString> generatePdf(const InvoiceDetails& invoice,
                                       const CompanyInfo& company,
                                       const QString& outputDir);
};

} // namespace nursera
