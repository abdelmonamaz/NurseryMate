#pragma once

#include "common/money.h"
#include "models/sale.h"

#include <QList>
#include <QString>

namespace nursera {

// Ligne de récapitulatif TVA par taux (facture conforme, F05-03).
struct VatBreakdownRow
{
    int ratePercent = 0;
    Money baseHt;
    Money vat;
};

// Facture émise depuis une vente.
struct Invoice
{
    int id = 0;
    QString uuid;
    QString number; // F-AAAA-NNNNN
    int saleId = 0;
    int customerId = 0;
    QString customerName;
    QString customerTaxId;
    Money subtotalHt;
    Money vatTotal;
    Money stampDuty;
    Money total; // TTC + timbre
    QString issuedAt;
};

// Détail complet pour le rendu PDF (facture + lignes issues de la vente).
struct InvoiceDetails
{
    Invoice header;
    QString saleNumber;
    Money globalDiscount;
    QList<SaleDetailLine> lines;    // HT par ligne (proratisé si remise)
    QList<VatBreakdownRow> vatRows; // récap TVA par taux
};

} // namespace nursera
