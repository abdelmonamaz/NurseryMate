#pragma once

#include "common/result.h"
#include "models/supplier.h"

#include <QList>

namespace nursera {

class ISupplierRepository
{
public:
    virtual ~ISupplierRepository() = default;

    virtual Result<QList<Supplier>> search(const QString& term,
                                           bool includeInactive = false) = 0;
    virtual Result<int> insert(const Supplier& supplier) = 0;
    virtual Result<void> update(const Supplier& supplier) = 0;
    virtual Result<void> setActive(int id, bool active) = 0;

    // Le fournisseur apparaît-il dans une transaction (réception, commande) ?
    virtual Result<bool> isReferenced(int id) = 0;
    // Suppression physique — ajout fautif jamais référencé uniquement.
    virtual Result<void> remove(int id) = 0;

    // Réception directe (F07-04) — UNE transaction : entête + lignes +
    // entrées de stock ref_kind='purchase' + mise à jour du coût moyen
    // pondéré des variantes (F03-09).
    virtual Result<int> recordReceipt(const ReceiptDraft& draft) = 0;

    // Journal des réceptions, plus récentes d'abord.
    virtual Result<QList<ReceiptRow>> recentReceipts(int limit = 50) = 0;

    // ── Paiements fournisseurs (F07-05) ───────────────────────

    // Paiement à un fournisseur. chequeDue : échéance ISO si chèque.
    virtual Result<int> recordSupplierPayment(int supplierId, Money amount,
                                              const QString& method,
                                              const QString& chequeNumber,
                                              const QString& chequeDue,
                                              const QString& note,
                                              int userId) = 0;
    // Journal des paiements d'un fournisseur, plus récents d'abord.
    virtual Result<QList<SupplierPaymentRow>> paymentsOf(int supplierId,
                                                         int limit = 30) = 0;
    // Dette : SUM(réceptions) − SUM(paiements).
    virtual Result<Money> supplierBalance(int supplierId) = 0;
    // Chèques dont l'échéance tombe d'ici daysAhead jours (échus inclus),
    // plus urgents d'abord.
    virtual Result<QList<DueChequeRow>> dueCheques(int daysAhead = 30) = 0;

    // ── Catalogue fournisseur (F07-06/07/09/10) ───────────────

    // Lien déclaratif fournisseur -> variante (doublon = no-op).
    virtual Result<void> linkProduct(int supplierId, int variantId) = 0;
    virtual Result<void> unlinkProduct(int supplierId, int variantId) = 0;

    // Produits d'un fournisseur : liens déclarés + produits réellement
    // livrés (réceptions), avec quantités et prix calculés.
    virtual Result<QList<SupplierProductRow>> productsOf(int supplierId) = 0;

    // « Meilleure offre » : les fournisseurs d'une variante, triés par
    // dernier prix d'achat croissant (les jamais-livrés en dernier).
    virtual Result<QList<SupplierOfferRow>> suppliersFor(int variantId) = 0;

    // Historique chronologique des prix d'achat d'une variante (courbe).
    virtual Result<QList<PricePoint>> priceHistoryOf(int variantId,
                                                     int limit = 30) = 0;

    // ── Commandes d'achat formelles (F07-02/03) ───────────────

    // Crée une commande au statut 'draft' (numéro BC-AAAA-NNN sans trou).
    virtual Result<PurchaseOrder> createOrder(const PurchaseOrderDraft& draft) = 0;

    // Journal des commandes, plus récentes d'abord. includeClosed = false
    // masque les commandes reçues et annulées.
    virtual Result<QList<PoRow>> recentOrders(bool includeClosed = true,
                                              int limit = 50) = 0;

    virtual Result<PoDetail> orderDetail(int poId) = 0;

    // Transition de statut (draft->sent, ->cancelled). La réception gère
    // partial/received automatiquement.
    virtual Result<void> setOrderStatus(int poId, const QString& status) = 0;

    // Réception (partielle ou totale) d'une commande (F07-03) — UNE
    // transaction : réception liée (po_id) + entrées stock ref_kind='purchase'
    // + CMP + incrément qty_received + recalcul du statut de la commande.
    virtual Result<int> receiveOrder(const PoReceiptDraft& draft) = 0;
};

} // namespace nursera
