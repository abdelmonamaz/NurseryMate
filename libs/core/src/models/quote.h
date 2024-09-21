#pragma once

#include "common/money.h"

#include <QList>
#include <QString>

namespace nursera {

enum class QuoteStatus { Draft, Sent, Accepted, Refused, Expired };

// Ligne de devis : produit (variantId) ou prestation libre (variantId = 0).
struct QuoteLine
{
    int variantId = 0;
    QString label;
    int qty = 1;
    Money unitPrice;

    Money lineTotal() const { return unitPrice * qty; }
};

struct QuoteDraft
{
    int customerId = 0;
    QString customerName; // snapshot (client libre possible)
    Money discount;
    QString validUntil; // ISO date, vide = 30 j par défaut
    QString note;
    int userId = 0;
    QList<QuoteLine> lines;

    Money subtotal() const
    {
        Money total;
        for (const QuoteLine& line : lines)
            total = total + line.lineTotal();
        return total;
    }
    Money total() const { return subtotal() - discount; }
};

struct Quote
{
    int id = 0;
    QString uuid;
    QString number; // D-AAAA-NNNNN
    Money total;
};

// Ligne du journal des devis.
struct QuoteRow
{
    int id = 0;
    QString number;
    QString customerName;
    QuoteStatus status = QuoteStatus::Draft;
    Money total;
    QString createdAt;
    QString validUntil;
};

// Détail pour le rendu PDF.
struct QuoteDetails
{
    int id = 0;
    QString number;
    int customerId = 0; // 0 = prospect sans fiche
    QString customerName;
    QString customerTaxId;
    QuoteStatus status = QuoteStatus::Draft;
    Money subtotal;
    Money discount;
    Money total;
    QString createdAt;
    QString validUntil;
    QString note;
    QList<QuoteLine> lines;
};

} // namespace nursera
