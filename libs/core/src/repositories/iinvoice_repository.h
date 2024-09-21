#pragma once

#include "common/result.h"
#include "models/invoice.h"

#include <QList>

namespace nursera {

class IInvoiceRepository
{
public:
    virtual ~IInvoiceRepository() = default;

    // Émet (ou retourne si déjà émise) la facture d'une vente complétée.
    // Numéro F-AAAA-NNNNN sans trou, snapshot client, timbre fiscal.
    virtual Result<Invoice> createFromSale(int saleId, Money stampDuty,
                                           int userId) = 0;

    // Détail complet pour le rendu (lignes HT proratisées, récap TVA).
    virtual Result<InvoiceDetails> details(int invoiceId) = 0;

    // Facture existante d'une vente, ou id 0 si aucune.
    virtual Result<int> invoiceIdForSale(int saleId) = 0;

    virtual Result<void> setPdfPath(int invoiceId, const QString& path) = 0;
};

} // namespace nursera
