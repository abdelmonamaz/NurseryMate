#pragma once

#include "common/result.h"
#include "generators/invoice_generator.h"
#include "models/sale.h"

#include <QString>

namespace nursera {

// Avoir PDF A4 (norme : contre-passation documentée, jamais de
// suppression) : en-tête société, référence de la vente contre-passée,
// lignes reprises, motif, mode de remboursement, total en lettres.
class CreditNoteGenerator
{
public:
    static Result<QString> generatePdf(const CreditNoteDetails& note,
                                       const InvoiceGenerator::CompanyInfo& company,
                                       const QString& outputDir);
};

} // namespace nursera
