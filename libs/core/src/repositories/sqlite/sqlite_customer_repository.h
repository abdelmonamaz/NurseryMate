#pragma once

#include "repositories/icustomer_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteCustomerRepository : public ICustomerRepository
{
public:
    explicit SqliteCustomerRepository(QString connectionName);

    Result<QList<CustomerRow>> search(const QString& term,
                                      bool includeInactive = false) override;
    Result<Customer> byId(int id) override;
    Result<int> insert(const Customer& customer) override;
    Result<void> update(const Customer& customer) override;
    Result<void> setActive(int id, bool active) override;
    Result<bool> isReferenced(int id) override;
    Result<void> remove(int id) override;
    Result<bool> phoneExists(const QString& phone) override;

    Result<Money> balanceOf(int customerId) override;
    Result<void> recordPayment(int customerId, Money amount,
                               PaymentMethod method,
                               const QString& chequeNumber,
                               const QString& chequeBank,
                               int userId) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
