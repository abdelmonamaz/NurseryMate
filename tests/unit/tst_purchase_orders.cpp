#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_supplier_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestPurchaseOrders : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createAssignsSequentialNumber();
    void rejectsDuplicateVariant();
    void cannotReceiveBeforeSending();
    void partialThenFullReceiptUpdatesStockAndStatus();
    void overReceiveRejected();
    void cancelBlocksReception();
    void reopenToDraftOnlyIfNothingReceived();

private:
    int addProduct(const QString& name, qint64 price);

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSupplierRepository* m_repo = nullptr;
    int m_supplier = 0;
    int m_vente = 0;
    int m_olivier = 0;
    int m_romarin = 0;

    int variantStock(int variantId)
    {
        QSqlQuery q(m_db->database());
        q.prepare(QStringLiteral(
            "SELECT COALESCE(SUM(qty), 0) FROM stock WHERE variant_id = :v"));
        q.bindValue(QStringLiteral(":v"), variantId);
        q.exec();
        q.next();
        return q.value(0).toInt();
    }
    qint64 avgCost(int variantId)
    {
        QSqlQuery q(m_db->database());
        q.prepare(QStringLiteral("SELECT avg_cost FROM variants WHERE id = :v"));
        q.bindValue(QStringLiteral(":v"), variantId);
        q.exec();
        q.next();
        return q.value(0).toLongLong();
    }
};

int TestPurchaseOrders::addProduct(const QString& name, qint64 price)
{
    SqliteProductRepository products(m_db->connectionName());
    Product product;
    product.nameFr = name;
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(price);
    const int productId = products.insertWithVariants(product, {godet}).value();
    return products.variantsOf(productId).value().first().id;
}

void TestPurchaseOrders::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("po.db")),
                               QStringLiteral("tst_po"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_repo = new SqliteSupplierRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id;

    Supplier supplier;
    supplier.name = QStringLiteral("Pépinières du Cap Bon");
    m_supplier = m_repo->insert(supplier).value();

    m_olivier = addProduct(QStringLiteral("Olivier"), 35000);
    m_romarin = addProduct(QStringLiteral("Romarin"), 3500);
}

void TestPurchaseOrders::createAssignsSequentialNumber()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_olivier;
    line.label = QStringLiteral("Olivier — godet");
    line.qtyOrdered = 10;
    line.unitCost = Money::fromMillimes(20000);
    draft.lines = {line};

    const auto first = m_repo->createOrder(draft);
    QVERIFY2(first.isOk(),
             qPrintable(first.isOk() ? QString() : first.error().message));
    const int year = QDate::currentDate().year();
    QCOMPARE(first.value().number,
             QStringLiteral("BC-%1-001").arg(year));
    QCOMPARE(first.value().status, QStringLiteral("draft"));
    QCOMPARE(first.value().totalCost, Money::fromMillimes(200000));

    const auto second = m_repo->createOrder(draft);
    QVERIFY(second.isOk());
    QCOMPARE(second.value().number, QStringLiteral("BC-%1-002").arg(year));

    // Les deux commandes brouillon apparaissent dans le journal.
    const auto orders = m_repo->recentOrders();
    QVERIFY(orders.isOk());
    QCOMPARE(orders.value().size(), 2);
    QCOMPARE(orders.value().at(0).number, QStringLiteral("BC-%1-002").arg(year));
    QCOMPARE(orders.value().at(0).qtyOrdered, 10);
    QCOMPARE(orders.value().at(0).qtyReceived, 0);
}

void TestPurchaseOrders::rejectsDuplicateVariant()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine a;
    a.variantId = m_olivier;
    a.qtyOrdered = 5;
    a.unitCost = Money::fromMillimes(20000);
    PoLine b = a;
    draft.lines = {a, b};
    const auto res = m_repo->createOrder(draft);
    QVERIFY(!res.isOk());
    QCOMPARE(res.error().code, QStringLiteral("po.duplicate"));
}

void TestPurchaseOrders::cannotReceiveBeforeSending()
{
    // Commande dédiée pour ce test.
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_romarin;
    line.label = QStringLiteral("Romarin — godet");
    line.qtyOrdered = 8;
    line.unitCost = Money::fromMillimes(2000);
    draft.lines = {line};
    const auto created = m_repo->createOrder(draft);
    QVERIFY2(created.isOk(),
             qPrintable(created.isOk() ? QString() : created.error().message));
    const int poId = created.value().id;

    PoReceiptDraft recv;
    recv.poId = poId;
    recv.locationId = m_vente;
    recv.lines = {{m_romarin, 8}};
    const auto res = m_repo->receiveOrder(recv);
    QVERIFY(!res.isOk());
    QCOMPARE(res.error().code, QStringLiteral("po.notReceivable"));
}

void TestPurchaseOrders::partialThenFullReceiptUpdatesStockAndStatus()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_olivier;
    line.label = QStringLiteral("Olivier — godet");
    line.qtyOrdered = 10;
    line.unitCost = Money::fromMillimes(30000);
    draft.lines = {line};
    const auto created = m_repo->createOrder(draft);
    QVERIFY2(created.isOk(),
             qPrintable(created.isOk() ? QString() : created.error().message));
    const int poId = created.value().id;

    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("sent")).isOk());

    const int stockBefore = variantStock(m_olivier);

    // Réception partielle : 4 sur 10.
    PoReceiptDraft partial;
    partial.poId = poId;
    partial.locationId = m_vente;
    partial.lines = {{m_olivier, 4}};
    QVERIFY(m_repo->receiveOrder(partial).isOk());

    QCOMPARE(variantStock(m_olivier), stockBefore + 4);
    // CMP mis à jour à la réception (F03-09).
    QCOMPARE(avgCost(m_olivier), 30000);

    auto detail = m_repo->orderDetail(poId);
    QVERIFY(detail.isOk());
    QCOMPARE(detail.value().status, QStringLiteral("partial"));
    QCOMPARE(detail.value().lines.first().qtyReceived, 4);
    QCOMPARE(detail.value().lines.first().qtyRemaining(), 6);

    // Réception du solde : 6 restants.
    PoReceiptDraft rest;
    rest.poId = poId;
    rest.locationId = m_vente;
    rest.lines = {{m_olivier, 6}};
    QVERIFY(m_repo->receiveOrder(rest).isOk());

    QCOMPARE(variantStock(m_olivier), stockBefore + 10);
    detail = m_repo->orderDetail(poId);
    QCOMPARE(detail.value().status, QStringLiteral("received"));
    QCOMPARE(detail.value().lines.first().qtyRemaining(), 0);

    // Une commande reçue n'est plus reçevable.
    PoReceiptDraft again;
    again.poId = poId;
    again.locationId = m_vente;
    again.lines = {{m_olivier, 1}};
    QVERIFY(!m_repo->receiveOrder(again).isOk());
}

void TestPurchaseOrders::overReceiveRejected()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_romarin;
    line.label = QStringLiteral("Romarin — godet");
    line.qtyOrdered = 5;
    line.unitCost = Money::fromMillimes(2000);
    draft.lines = {line};
    const auto created = m_repo->createOrder(draft);
    QVERIFY2(created.isOk(),
             qPrintable(created.isOk() ? QString() : created.error().message));
    const int poId = created.value().id;
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("sent")).isOk());

    const int stockBefore = variantStock(m_romarin);
    PoReceiptDraft recv;
    recv.poId = poId;
    recv.locationId = m_vente;
    recv.lines = {{m_romarin, 6}}; // > 5 commandés
    const auto res = m_repo->receiveOrder(recv);
    QVERIFY(!res.isOk());
    QCOMPARE(res.error().code, QStringLiteral("po.overReceive"));
    // Rien encaissé : stock inchangé.
    QCOMPARE(variantStock(m_romarin), stockBefore);
}

void TestPurchaseOrders::cancelBlocksReception()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_romarin;
    line.label = QStringLiteral("Romarin — godet");
    line.qtyOrdered = 3;
    line.unitCost = Money::fromMillimes(2000);
    draft.lines = {line};
    const auto created = m_repo->createOrder(draft);
    QVERIFY2(created.isOk(),
             qPrintable(created.isOk() ? QString() : created.error().message));
    const int poId = created.value().id;

    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("cancelled")).isOk());
    // Une commande annulée ne peut pas passer à 'sent'.
    QVERIFY(!m_repo->setOrderStatus(poId, QStringLiteral("sent")).isOk());

    PoReceiptDraft recv;
    recv.poId = poId;
    recv.locationId = m_vente;
    recv.lines = {{m_romarin, 3}};
    QVERIFY(!m_repo->receiveOrder(recv).isOk());

    // Masquée quand includeClosed = false.
    const auto open = m_repo->recentOrders(/*includeClosed=*/false);
    QVERIFY(open.isOk());
    for (const PoRow& row : open.value())
        QVERIFY(row.status != QLatin1String("cancelled"));
}

void TestPurchaseOrders::reopenToDraftOnlyIfNothingReceived()
{
    PurchaseOrderDraft draft;
    draft.supplierId = m_supplier;
    PoLine line;
    line.variantId = m_romarin;
    line.label = QStringLiteral("Romarin — godet");
    line.qtyOrdered = 6;
    line.unitCost = Money::fromMillimes(2000);
    draft.lines = {line};

    // Envoyée par erreur -> retour au brouillon possible tant que rien reçu.
    const int poId = m_repo->createOrder(draft).value().id;
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("sent")).isOk());
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("draft")).isOk());
    QCOMPARE(m_repo->orderDetail(poId).value().status, QStringLiteral("draft"));

    // Annulée par erreur -> rouvrable aussi.
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("cancelled")).isOk());
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("draft")).isOk());

    // Mais dès qu'une réception a eu lieu, le retour au brouillon est refusé.
    QVERIFY(m_repo->setOrderStatus(poId, QStringLiteral("sent")).isOk());
    PoReceiptDraft recv;
    recv.poId = poId;
    recv.locationId = m_vente;
    recv.lines = {{m_romarin, 2}};
    QVERIFY(m_repo->receiveOrder(recv).isOk());
    const auto blocked = m_repo->setOrderStatus(poId, QStringLiteral("draft"));
    QVERIFY(!blocked.isOk());
    QCOMPARE(blocked.error().code, QStringLiteral("po.received"));
}

QTEST_GUILESS_MAIN(TestPurchaseOrders)
#include "tst_purchase_orders.moc"
