#pragma once

#include "common/money.h"

#include <QList>
#include <QString>

namespace nursera {

// Fournisseur (F07-01).
struct Supplier
{
    int id = 0;
    QString name;
    QString phone;
    QString email;
    QString address;   // adresse exacte (base de la future localisation carte)
    QString taxId;
    QString paymentTerms;
    QString notes;
    QString supplies;  // produits/services que ce fournisseur peut fournir
    // Coordonnées GPS pour la carte (feature future). 0/0 = non renseigné
    // (aucun fournisseur au large du golfe de Guinée).
    double latitude = 0;
    double longitude = 0;
    bool active = true;
    // Dette envers ce fournisseur (F07-05) : réceptions − paiements.
    // Calculée par search(), jamais stockée.
    Money balance;
};

// Paiement fait à un fournisseur (F07-05).
struct SupplierPaymentRow
{
    int id = 0;
    QString date;         // ISO
    Money amount;
    QString method;       // cash | cheque | transfer
    QString chequeNumber;
    QString chequeDue;    // échéance ISO, vide sinon
    QString note;
};

// Échéance de chèque à venir (tous fournisseurs confondus).
struct DueChequeRow
{
    QString supplierName;
    Money amount;
    QString chequeNumber;
    QString dueDate;
    bool overdue = false; // échéance dépassée
};

// Ligne d'une réception de marchandise (F07-04).
struct ReceiptLine
{
    int variantId = 0;
    QString label; // affichage
    int qty = 0;
    Money unitCost;

    Money lineCost() const { return unitCost * qty; }
};

// Réception directe prête à enregistrer.
struct ReceiptDraft
{
    int supplierId = 0; // 0 = achat sans fournisseur identifié
    int locationId = 0;
    QString note;
    int userId = 0;
    QList<ReceiptLine> lines;

    Money totalCost() const
    {
        Money total;
        for (const ReceiptLine& line : lines)
            total = total + line.lineCost();
        return total;
    }
};

// Ligne du journal des réceptions.
struct ReceiptRow
{
    int id = 0;
    QString receivedAt;
    QString supplierName;
    QString locationFr;
    int lineCount = 0;
    int totalQty = 0;
    Money totalCost;
};

// ── Catalogue fournisseur & approvisionnement (F07-06/07/09/10) ──

// Produit d'un fournisseur : lien déclaratif (supplier_products) fusionné
// avec les statistiques CALCULÉES depuis les réceptions.
struct SupplierProductRow
{
    int variantId = 0;
    QString label;        // "Citronnier 4 saisons — pot17"
    QString sku;
    bool linked = false;   // lien déclaré (sinon : détecté dans les réceptions)
    int qtySupplied = 0;   // total livré par CE fournisseur
    int deliveryCount = 0;
    Money lastCost;        // dernier prix d'achat facturé
    Money avgCost;         // prix moyen pondéré chez ce fournisseur
    QString lastDelivery;  // date ISO de la dernière livraison
};

// Offre d'un fournisseur pour UNE variante (comparaison « meilleure offre »).
struct SupplierOfferRow
{
    int supplierId = 0;
    QString supplierName;
    bool linked = false;
    int qtySupplied = 0;
    int deliveryCount = 0;
    Money lastCost;
    Money avgCost;
    QString lastDelivery;
};

// Point de l'historique des prix d'achat d'une variante (courbe F07-10).
struct PricePoint
{
    QString date;         // ISO
    Money unitCost;
    QString supplierName; // vide = achat sans fournisseur
};

// ── Commandes d'achat formelles (F07-02/03) ───────────────────

// Ligne de commande : quantité commandée + déjà reçue (contrôle F07-03).
struct PoLine
{
    int variantId = 0;
    QString label;         // snapshot au moment de la commande
    int qtyOrdered = 0;
    int qtyReceived = 0;
    Money unitCost;

    int qtyRemaining() const { return qMax(0, qtyOrdered - qtyReceived); }
    Money lineCost() const { return unitCost * qtyOrdered; }
};

// Brouillon de commande prêt à créer.
struct PurchaseOrderDraft
{
    int supplierId = 0; // obligatoire (> 0)
    QString note;
    int userId = 0;
    QList<PoLine> lines;

    Money totalCost() const
    {
        Money total;
        for (const PoLine& line : lines)
            total = total + line.lineCost();
        return total;
    }
};

// Commande créée (retour de createOrder).
struct PurchaseOrder
{
    int id = 0;
    QString uuid;
    QString number; // BC-AAAA-NNN
    int supplierId = 0;
    QString status = QStringLiteral("draft");
    Money totalCost;
};

// Ligne du journal des commandes.
struct PoRow
{
    int id = 0;
    QString number;
    QString createdAt;
    QString supplierName;
    QString status; // draft | sent | partial | received | cancelled
    int lineCount = 0;
    int qtyOrdered = 0;
    int qtyReceived = 0;
    Money totalCost;
};

// Détail d'une commande (pour la réception).
struct PoDetail
{
    int id = 0;
    QString number;
    QString supplierName;
    QString status;
    QList<PoLine> lines;
};

// Réception d'une commande : quantités reçues maintenant, par ligne.
struct PoReceiptLine
{
    int variantId = 0;
    int qty = 0;
};

struct PoReceiptDraft
{
    int poId = 0;
    int locationId = 0;
    QString note;
    int userId = 0;
    QList<PoReceiptLine> lines;
};

} // namespace nursera
