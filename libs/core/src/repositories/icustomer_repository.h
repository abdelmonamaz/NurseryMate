#pragma once

#include "common/result.h"
#include "models/customer.h"
#include "models/sale.h" // PaymentMethod

#include <QList>

namespace nursera {

class ICustomerRepository
{
public:
    virtual ~ICustomerRepository() = default;

    // Recherche nom / téléphone (F06-04), avec encours agrégé.
    virtual Result<QList<CustomerRow>> search(const QString& term,
                                              bool includeInactive = false) = 0;
    virtual Result<Customer> byId(int id) = 0;
    virtual Result<int> insert(const Customer& customer) = 0;
    virtual Result<void> update(const Customer& customer) = 0;
    // Désactivation, jamais de suppression (RG-06.a).
    virtual Result<void> setActive(int id, bool active) = 0;

    // Le client apparaît-il dans une transaction (vente, paiement, devis) ?
    virtual Result<bool> isReferenced(int id) = 0;
    // Suppression physique — admise par la norme UNIQUEMENT pour une fiche
    // jamais référencée (ajout fautif). Refusée sinon.
    virtual Result<void> remove(int id) = 0;
    // Détection de doublon à la création (F06-04).
    virtual Result<bool> phoneExists(const QString& phone) = 0;

    // Encours : SUM(ventes) - SUM(paiements) du client (F06-03).
    virtual Result<Money> balanceOf(int customerId) = 0;
    // Règlement sur encours (sale_id NULL).
    virtual Result<void> recordPayment(int customerId, Money amount,
                                       PaymentMethod method,
                                       const QString& chequeNumber,
                                       const QString& chequeBank,
                                       int userId) = 0;
};

} // namespace nursera
