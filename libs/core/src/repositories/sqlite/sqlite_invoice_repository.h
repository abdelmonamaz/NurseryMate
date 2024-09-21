#pragma once

#include "repositories/iinvoice_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteInvoiceRepository : public IInvoiceRepository
{
public:
    explicit SqliteInvoiceRepository(QString connectionName);

    Result<Invoice> createFromSale(int saleId, Money stampDuty,
                                   int userId) override;
    Result<InvoiceDetails> details(int invoiceId) override;
    Result<int> invoiceIdForSale(int saleId) override;
    Result<void> setPdfPath(int invoiceId, const QString& path) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    Result<QString> nextInvoiceNumber(); // dans la transaction

    QString m_connectionName;
};

} // namespace nursera
