#pragma once

#include "common/money.h"

#include <QList>
#include <QString>

namespace nursera {

enum class PaymentMethod { Cash, Cheque, Transfer };

// Ligne du panier / de la vente. Le libellé et le prix sont figés au
// moment de la vente (snapshot, doc 02 §3.2).
struct SaleLine
{
    int variantId = 0;
    QString label;      // "Romarin — godet"
    int qty = 1;
    Money unitPrice;    // prix effectif (particulier ou pro selon le client)
    int vatRatePercent = 0;
    int discountBp = 0; // remise ligne en points de base (V1)

    // Tarifs candidats conservés pour la re-tarification pro (RG-04.c).
    Money unitPriceRegular;
    Money unitPricePro; // 0 = pas de tarif pro
    // Prix négocié à la main (F04-01) : ne pas écraser par la bascule pro.
    bool manualPrice = false;

    Money lineTotal() const { return unitPrice * qty; }
};

// Brouillon de vente prêt à encaisser (construit par le controller).
struct SaleDraft
{
    QList<SaleLine> lines;
    Money globalDiscount;
    PaymentMethod method = PaymentMethod::Cash;
    QString chequeNumber;
    QString chequeBank;
    QString chequeDue;
    int userId = 0;
    int stockLocationId = 0; // emplacement décrémenté (RG-04.a)
    int customerId = 0;      // 0 = client de passage (F04-04)
    bool onCredit = false;   // vente à crédit : client obligatoire (F04-05)

    Money subtotal() const
    {
        Money total;
        for (const SaleLine& line : lines)
            total = total + line.lineTotal();
        return total;
    }
    Money total() const { return subtotal() - globalDiscount; }
};

// Vente enregistrée (retour de ISaleRepository::record).
struct Sale
{
    int id = 0;
    QString uuid;
    QString number; // T-AAAA-NNNNN (RG-04.b)
    Money total;
};

// Ligne du journal des ventes (F04-07).
struct SaleJournalRow
{
    int id = 0;
    QString number;
    QString createdAt;
    Money total;
    QString method;
    QString userName;
    QString status;
    QString creditNoteNumber; // dernier avoir (AV-AAAA-NNN), sinon vide
    int creditNoteCount = 0;
    Money refundedTotal; // somme des avoirs — < total => encore remboursable
};

// Avoir / note de crédit (norme : une vente ne se supprime jamais,
// elle se contre-passe). Depuis la V1.5, l'avoir peut être PARTIEL :
// lignes choisies, quantités partielles, plusieurs avoirs par vente
// tant que tout n'est pas remboursé.
struct CreditNoteLineDraft
{
    int saleLineId = 0;
    int qty = 0;
};

struct CreditNoteDraft
{
    int saleId = 0;
    QString reason;      // obligatoire — pourquoi cette contre-passation
    bool restock = true; // la marchandise revient en stock (retour / saisie fictive)
    // cash|cheque|transfer = remboursé au client · credit = imputé sur l'encours
    QString refundMethod = QStringLiteral("cash");
    int stockLocationId = 0; // destination du retour si restock
    int userId = 0;
    // Vide = avoir TOTAL (tout le restant remboursable).
    QList<CreditNoteLineDraft> lines;
};

// Ligne d'une vente avec son déjà-remboursé (préparation d'un avoir).
struct RefundableLine
{
    int saleLineId = 0;
    int variantId = 0; // 0 = prestation libre
    QString label;
    int qtySold = 0;
    int qtyRefunded = 0;
    Money unitPrice;
    int remaining() const { return qtySold - qtyRefunded; }
};

struct CreditNote
{
    int id = 0;
    QString uuid;
    QString number; // AV-AAAA-NNN
    Money total;
};

// Détail complet d'une vente (ticket, F04-06).
struct SaleDetailLine
{
    QString label;
    int qty = 0;
    Money unitPrice;
    Money lineTotal;
};

struct SaleDetails
{
    int id = 0;
    QString number;
    QString createdAt;
    QString userName;
    QString method;
    Money subtotal;
    Money discount;
    Money vatTotal;
    Money total;
    QList<SaleDetailLine> lines;
};

// Détail complet d'un avoir (PDF AV-AAAA-NNN) : entête + LIGNES
// REMBOURSÉES par cet avoir (partiel ou total) + vente de référence.
struct CreditNoteDetails
{
    QString number; // AV-AAAA-NNN
    QString createdAt;
    QString reason;
    bool restock = true;
    QString refundMethod; // cash|cheque|transfer|credit
    Money total;          // lignes − remise reprise au prorata
    QString userName;
    QList<SaleDetailLine> lines; // ce que CET avoir rembourse
    SaleDetails sale;
};

// Totaux du jour par mode de paiement.
struct DayTotals
{
    int saleCount = 0;
    Money total;
    Money cash;
    Money cheque;
    Money transfer;
};

} // namespace nursera
