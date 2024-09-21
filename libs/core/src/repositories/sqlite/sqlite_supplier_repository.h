#pragma once

#include "repositories/istock_repository.h"
#include "repositories/isupplier_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteSupplierRepository : public ISupplierRepository
{
public:
    // Dépend de IStockRepository : les entrées de stock de la réception
    // passent par recordMove(ownTransaction=false).
    SqliteSupplierRepository(QString connectionName, IStockRepository& stock);

    Result<QList<Supplier>> search(const QString& term,
                                   bool includeInactive = false) override;
    Result<int> insert(const Supplier& supplier) override;
    Result<void> update(const Supplier& supplier) override;
    Result<void> setActive(int id, bool active) override;
    Result<bool> isReferenced(int id) override;
    Result<void> remove(int id) override;

    Result<int> recordReceipt(const ReceiptDraft& draft) override;
    Result<QList<ReceiptRow>> recentReceipts(int limit = 50) override;

    Result<int> recordSupplierPayment(int supplierId, Money amount,
                                      const QString& method,
                                      const QString& chequeNumber,
                                      const QString& chequeDue,
                                      const QString& note,
                                      int userId) override;
    Result<QList<SupplierPaymentRow>> paymentsOf(int supplierId,
                                                 int limit = 30) override;
    Result<Money> supplierBalance(int supplierId) override;
    Result<QList<DueChequeRow>> dueCheques(int daysAhead = 30) override;

    Result<void> linkProduct(int supplierId, int variantId) override;
    Result<void> unlinkProduct(int supplierId, int variantId) override;
    Result<QList<SupplierProductRow>> productsOf(int supplierId) override;
    Result<QList<SupplierOfferRow>> suppliersFor(int variantId) override;
    Result<QList<PricePoint>> priceHistoryOf(int variantId,
                                             int limit = 30) override;

    Result<PurchaseOrder> createOrder(const PurchaseOrderDraft& draft) override;
    Result<QList<PoRow>> recentOrders(bool includeClosed = true,
                                      int limit = 50) override;
    Result<PoDetail> orderDetail(int poId) override;
    Result<void> setOrderStatus(int poId, const QString& status) override;
    Result<int> receiveOrder(const PoReceiptDraft& draft) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }
    // CMP (F03-09) : nouveau coût = (stock*ancien + qty*coût) / (stock+qty).
    Result<void> updateAverageCost(int variantId, int qty, qint64 unitCost);

    QString m_connectionName;
    IStockRepository& m_stock;
};

} // namespace nursera
