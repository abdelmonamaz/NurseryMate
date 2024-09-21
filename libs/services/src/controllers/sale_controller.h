#pragma once

#include "qml_models/cart_model.h"
#include "repositories/icustomer_repository.h"
#include "repositories/ilocation_repository.h"
#include "repositories/iquote_repository.h"
#include "repositories/isale_repository.h"
#include "repositories/isettings_repository.h"
#include "repositories/istock_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de la caisse (M04) : panier, remise globale, encaissement,
// journal du jour (F04-01..07).
class SaleController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(nursera::CartModel* cart READ cart CONSTANT)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY cartChanged)
    Q_PROPERTY(QString subtotalDisplay READ subtotalDisplay NOTIFY cartChanged)
    Q_PROPERTY(QString discountDisplay READ discountDisplay NOTIFY cartChanged)
    Q_PROPERTY(QString totalDisplay READ totalDisplay NOTIFY cartChanged)
    Q_PROPERTY(bool hasDiscount READ hasDiscount NOTIFY cartChanged)
    Q_PROPERTY(int cartCustomerId READ cartCustomerId NOTIFY customerChanged)
    Q_PROPERTY(QString cartCustomerName READ cartCustomerName NOTIFY customerChanged)
    Q_PROPERTY(bool proPricing READ proPricing NOTIFY customerChanged)
    Q_PROPERTY(QVariantList heldCarts READ heldCarts NOTIFY heldChanged)

public:
    SaleController(ISaleRepository& sales,
                   IStockRepository& stock,
                   ILocationRepository& locations,
                   QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    // Active la génération de tickets PDF (F04-06) et des PDF d'avoir
    // (creditNoteDir vide = même dossier que les tickets).
    void configureTickets(ISettingsRepository* settings, const QString& outputDir,
                          const QString& creditNoteDir = QString())
    {
        m_settings = settings;
        m_ticketDir = outputDir;
        m_creditNoteDir = creditNoteDir.isEmpty() ? outputDir : creditNoteDir;
    }

    // Active la vente rattachée à un client / à crédit (F04-04, F04-05).
    // Conversion devis accepté -> panier (F05) : accès en lecture aux devis.
    void configureQuotes(IQuoteRepository* quotes) { m_quotes = quotes; }

    void configureCustomers(ICustomerRepository* customers)
    {
        m_customers = customers;
    }

    CartModel* cart() { return &m_cart; }
    int itemCount() const { return m_cart.itemCount(); }
    QString subtotalDisplay() const;
    QString discountDisplay() const;
    QString totalDisplay() const;
    bool hasDiscount() const { return m_globalDiscount.millimes() > 0; }
    Money total() const { return m_cart.subtotal() - m_globalDiscount; }
    int cartCustomerId() const { return m_cartCustomerId; }
    QString cartCustomerName() const { return m_cartCustomerName; }
    bool proPricing() const { return m_cartIsPro; }

    // Recherche produit pour la caisse (texte ou douchette).
    Q_INVOKABLE QVariantList searchVariants(const QString& term) const;

    // Recherche client pour le rattachement de la vente (F04-04).
    Q_INVOKABLE QVariantList searchCustomers(const QString& term) const;

    // Rattache le panier à un client (0 = passage) ; applique le tarif
    // pro si le client est professionnel (RG-04.c).
    Q_INVOKABLE void setCartCustomer(int customerId);

    // pick = élément retourné par searchVariants.
    Q_INVOKABLE void addToCart(const QVariantMap& pick);
    Q_INVOKABLE void setQty(int row, int qty);
    // Prix unitaire négocié à la main sur une ligne (F04-01).
    Q_INVOKABLE bool setLinePrice(int row, const QString& priceText);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void clearCart();

    // Paniers en attente (F04-10) : met le panier courant de côté et en
    // démarre un neuf ; reprend un panier plus tard (auto-met en attente
    // le panier courant s'il n'est pas vide).
    QVariantList heldCarts() const;
    Q_INVOKABLE void holdCart();
    Q_INVOKABLE void resumeCart(int index);

    // Charge un devis ACCEPTÉ dans le panier (le panier courant non vide
    // part en attente). Lignes produit + prestations libres ; le client du
    // devis est rattaché s'il existe.
    Q_INVOKABLE bool loadQuote(int quoteId);

    // Remise globale "3,500" — vide pour annuler (F04-02).
    Q_INVOKABLE bool setGlobalDiscount(const QString& amount);

    // Rendu monnaie : "50" donné -> "8,500 DT" (F04-03).
    Q_INVOKABLE QString changeFor(const QString& amountGiven) const;

    // data : method ("cash"|"cheque"|"transfer"), chequeNumber, chequeBank.
    // Émet saleCompleted(number, totalDisplay, ticketUrl) et vide le panier.
    Q_INVOKABLE bool checkout(const QVariantMap& data);

    // (Ré)génère le ticket PDF d'une vente ; retourne l'url file:// ou "".
    Q_INVOKABLE QString ticketFor(int saleId);

    // Annulation jour même, motif obligatoire (F04-08 — bouton Gérant).
    Q_INVOKABLE bool cancelSale(int saleId, const QString& reason);
    // Avoir (norme) : contre-passe une vente APRÈS le jour même.
    // data : saleId*, reason*, restock (défaut true), refundMethod
    // (cash|cheque|transfer|credit), locationId (si restock),
    // lines: [{saleLineId, qty}] — absent/vide = avoir TOTAL du restant.
    Q_INVOKABLE bool createCreditNote(const QVariantMap& data);
    // Lignes remboursables d'une vente : [{saleLineId, label, qtySold,
    // qtyRefunded, remaining, unitPriceDisplay}].
    Q_INVOKABLE QVariantList refundableLines(int saleId) const;
    // (Ré)génère le PDF de l'avoir d'une vente ; retourne l'url file:// ou "".
    Q_INVOKABLE QString creditNotePdfFor(int saleId);

    // Journal du jour (F04-07).
    Q_INVOKABLE QVariantList todayJournal() const;
    Q_INVOKABLE QVariantMap todayTotals() const;

    // Clôture de caisse (F04-09) : théorique espèces attendu, et
    // enregistrement du compté -> émet closureDone(gapDisplay, isGap).
    Q_INVOKABLE QString expectedCashDisplay() const;
    Q_INVOKABLE bool recordClosure(const QString& countedAmount);

signals:
    void cartChanged();
    void customerChanged();
    void heldChanged();
    void saleCompleted(const QString& number, const QString& totalDisplay,
                       const QString& ticketUrl);
    void saleCancelled();
    void creditNoteCreated(const QString& number, const QString& pdfUrl);
    void closureDone(const QString& gapDisplay, bool hasGap);
    void errorOccurred(const QString& message);

private:
    int salesLocationId() const;

    ISaleRepository& m_sales;
    IStockRepository& m_stock;
    ILocationRepository& m_locations;
    ICustomerRepository* m_customers = nullptr;
    ISettingsRepository* m_settings = nullptr;
    IQuoteRepository* m_quotes = nullptr;
    QString m_ticketDir;
    QString m_creditNoteDir;
    // Un panier mis de côté (F04-10) — état de session, non persisté.
    struct HeldCart
    {
        QList<SaleLine> lines;
        Money discount;
        int customerId = 0;
        QString customerName;
        bool isPro = false;
    };

    CartModel m_cart;
    Money m_globalDiscount;
    int m_cartCustomerId = 0;
    QString m_cartCustomerName;
    bool m_cartIsPro = false;
    QList<HeldCart> m_held;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
