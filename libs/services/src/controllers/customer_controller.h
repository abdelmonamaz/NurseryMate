#pragma once

#include "repositories/icustomer_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de l'écran Clients (M06) : fiches, encours, règlements.
class CustomerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList customers READ customers NOTIFY refreshed)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm
                   NOTIFY searchTermChanged)
    Q_PROPERTY(bool showInactive READ showInactive WRITE setShowInactive
                   NOTIFY showInactiveChanged)

public:
    explicit CustomerController(ICustomerRepository& customers,
                                QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    QVariantList customers() const { return m_rows; }
    QString searchTerm() const { return m_searchTerm; }
    void setSearchTerm(const QString& term);
    bool showInactive() const { return m_showInactive; }
    void setShowInactive(bool show);

    Q_INVOKABLE void refresh();

    // data : name*, kind ("individual"|"professional"), phone, creditLimit
    // ("500" DT, vide = pas de plafond). Doublon téléphone détecté (F06-04).
    Q_INVOKABLE bool createCustomer(const QVariantMap& data);

    // Fiche complète pour édition : { id, name, kind, phone, phone2, email,
    // address, taxId, notes, creditLimit ("" = pas de plafond) }.
    Q_INVOKABLE QVariantMap customerDetail(int customerId);
    // data : id* + mêmes champs que createCustomer (+ phone2, email, address,
    // taxId, notes). Doublon téléphone contrôlé hors fiche elle-même.
    Q_INVOKABLE bool updateCustomer(const QVariantMap& data);
    // Désactivation, jamais de suppression (RG-06.a).
    Q_INVOKABLE bool setCustomerActive(int customerId, bool active);
    // Ajout fautif jamais référencé : suppression physique admise (norme).
    Q_INVOKABLE bool isDeletable(int customerId);
    Q_INVOKABLE bool deleteCustomer(int customerId);

    // data : customerId, amount ("50,000"), method ("cash"|"cheque"|"transfer"),
    // chequeNumber, chequeBank — règlement sur encours (F06-03).
    Q_INVOKABLE bool recordPayment(const QVariantMap& data);

signals:
    void searchTermChanged();
    void showInactiveChanged();
    void refreshed();
    void customerCreated(int customerId);
    void customerUpdated();
    void paymentRecorded();
    void errorOccurred(const QString& message);

private:
    ICustomerRepository& m_customers;
    QVariantList m_rows;
    QString m_searchTerm;
    bool m_showInactive = false;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
