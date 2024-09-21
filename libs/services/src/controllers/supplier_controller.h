#pragma once

#include "repositories/isupplier_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de l'écran Achats (M07) : fournisseurs + réceptions directes.
class SupplierController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList suppliers READ suppliers NOTIFY refreshed)
    Q_PROPERTY(QVariantList receipts READ receipts NOTIFY refreshed)
    Q_PROPERTY(QVariantList orders READ orders NOTIFY refreshed)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm
                   NOTIFY searchTermChanged)
    Q_PROPERTY(bool showInactive READ showInactive WRITE setShowInactive
                   NOTIFY showInactiveChanged)

public:
    explicit SupplierController(ISupplierRepository& suppliers,
                                QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    QVariantList suppliers() const { return m_suppliers; }
    QVariantList receipts() const { return m_receipts; }
    QVariantList orders() const { return m_orders; }
    QString searchTerm() const { return m_searchTerm; }
    void setSearchTerm(const QString& term);
    bool showInactive() const { return m_showInactive; }
    void setShowInactive(bool show);

    Q_INVOKABLE void refresh();

    // data : name*, phone, email, address, taxId, paymentTerms, notes,
    //        supplies (produits/services fournis).
    Q_INVOKABLE bool createSupplier(const QVariantMap& data);
    // data : id* + mêmes champs que createSupplier.
    Q_INVOKABLE bool updateSupplier(const QVariantMap& data);
    // Désactivation, jamais de suppression (historique conservé).
    Q_INVOKABLE bool setSupplierActive(int supplierId, bool active);
    // Ajout fautif jamais référencé : suppression physique admise (norme).
    Q_INVOKABLE bool isDeletable(int supplierId);
    Q_INVOKABLE bool deleteSupplier(int supplierId);

    // data : supplierId (0 = sans fournisseur), locationId, note,
    //        lines: [{variantId, qty, cost ("12,500")}].
    Q_INVOKABLE bool recordReceipt(const QVariantMap& data);

    // ── Paiements fournisseurs (F07-05) ───────────────────────

    // data : supplierId*, amount* ("120,000"), method (cash|cheque|transfer),
    //        chequeNumber, chequeDue (ISO, requis si chèque), note.
    Q_INVOKABLE bool paySupplier(const QVariantMap& data);
    // Journal des paiements d'un fournisseur : [{date, amount, method,
    // chequeNumber, chequeDue, note}].
    Q_INVOKABLE QVariantList supplierPayments(int supplierId);
    // Dette "120,000 DT" (affichage) de ce fournisseur.
    Q_INVOKABLE QString supplierBalanceDisplay(int supplierId);
    // Échéances de chèques ≤ 30 j (échus inclus, plus urgents d'abord) :
    // [{supplierName, amount, chequeNumber, dueDate, overdue}].
    Q_INVOKABLE QVariantList dueCheques();

    // ── Catalogue fournisseur (F07-06/07/09/10) ───────────────

    // Produits d'un fournisseur (liés + livrés) avec stats calculées :
    // [{variantId, label, sku, linked, qtySupplied, deliveryCount,
    //   lastCost, avgCost, lastDelivery}].
    Q_INVOKABLE QVariantList supplierProducts(int supplierId);
    Q_INVOKABLE bool linkProduct(int supplierId, int variantId);
    Q_INVOKABLE bool unlinkProduct(int supplierId, int variantId);

    // « Meilleure offre » : fournisseurs d'une variante, meilleur dernier
    // prix d'abord : [{supplierId, supplierName, linked, qtySupplied,
    // deliveryCount, lastCost, avgCost, lastDelivery, best}].
    Q_INVOKABLE QVariantList offersFor(int variantId);

    // Courbe des prix d'achat : [{label (date), value (millimes),
    // display, fullLabel}] — branchable sur NLineChart/NBarChart.
    Q_INVOKABLE QVariantList priceHistory(int variantId);

    // Dernier prix d'achat "12,500" chez ce fournisseur ('' si aucun) —
    // pré-remplissage des lignes de commande (F07-08).
    Q_INVOKABLE QString lastCost(int supplierId, int variantId);

    // Préparation de commande : qui peut fournir ces lignes ?
    // lines : [{variantId, qty, label}] ; sortBy : "price" (couverture
    // puis total estimé croissant) | "quantity" (quantité déjà fournie
    // décroissante). Distance : viendra avec la carte (lat/lng prêts).
    // Retour : [{supplierId, name, covered, total, full, coverageLabel,
    //   estimatedDisplay, suppliedQty, missingLabels, priceComplete}].
    Q_INVOKABLE QVariantList supplierMatches(const QVariantList& lines,
                                             const QString& sortBy);

    // ── Commandes d'achat (F07-02/03) ─────────────────────────

    // data : supplierId*, note, lines: [{variantId, label, qty, cost}].
    Q_INVOKABLE bool createOrder(const QVariantMap& data);
    // Détail d'une commande pour la réception : { number, supplierName,
    // status, lines: [{variantId, label, qtyOrdered, qtyReceived, remaining,
    // unitCost}] }.
    Q_INVOKABLE QVariantMap orderDetail(int poId);
    Q_INVOKABLE bool sendOrder(int poId);
    Q_INVOKABLE bool cancelOrder(int poId);
    // Retour au brouillon (fausse manip) — refusé si déjà reçue en partie.
    Q_INVOKABLE bool reopenOrder(int poId);
    // data : poId*, locationId*, note, lines: [{variantId, qty}].
    Q_INVOKABLE bool receiveOrder(const QVariantMap& data);

signals:
    void searchTermChanged();
    void showInactiveChanged();
    void refreshed();
    void supplierCreated(int supplierId);
    void supplierUpdated();
    void supplierPaid();
    void receiptRecorded();
    void orderCreated(int poId);
    void orderReceived();
    void errorOccurred(const QString& message);

private:
    ISupplierRepository& m_repository;
    QVariantList m_suppliers;
    QVariantList m_receipts;
    QVariantList m_orders;
    QString m_searchTerm;
    bool m_showInactive = false;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
