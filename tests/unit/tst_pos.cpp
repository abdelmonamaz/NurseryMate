#include "controllers/sale_controller.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_customer_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_quote_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

// Vérifie la tarification pro automatique au niveau du controller (RG-04.c).
class TestPos : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void proPricingSwitchesOnProCustomer();
    void negotiatedLinePriceSurvivesReprice();
    void holdAndResumeCart();
    void loadAcceptedQuoteIntoCart();

private:
    QVariantMap pick(int variantId) const;

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    SqliteLocationRepository* m_locations = nullptr;
    SqliteCustomerRepository* m_customers = nullptr;
    SaleController* m_pos = nullptr;
    int m_variantId = 0;
    int m_particulier = 0;
    int m_pro = 0;
};

QVariantMap TestPos::pick(int variantId) const
{
    return QVariantMap{
        {QStringLiteral("variantId"), variantId},
        {QStringLiteral("label"), QStringLiteral("Citronnier — pot21")},
        {QStringLiteral("priceMillimes"), 25000},
        {QStringLiteral("priceProMillimes"), 20000},
        {QStringLiteral("vatRate"), 19},
    };
}

void TestPos::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("pos.db")),
                               QStringLiteral("tst_pos"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);
    m_locations = new SqliteLocationRepository(m_db->connectionName());
    m_customers = new SqliteCustomerRepository(m_db->connectionName());
    m_pos = new SaleController(*m_sales, *m_stock, *m_locations);
    m_pos->configureCustomers(m_customers);

    SqliteProductRepository products(m_db->connectionName());
    Product citronnier;
    citronnier.nameFr = QStringLiteral("Citronnier");
    Variant pot21;
    pot21.packaging = QStringLiteral("pot21");
    pot21.priceTtc = Money::fromMillimes(25000);
    pot21.priceProTtc = Money::fromMillimes(20000);
    const int pid = products.insertWithVariants(citronnier, {pot21}).value();
    m_variantId = products.variantsOf(pid).value().first().id;

    Customer particulier;
    particulier.name = QStringLiteral("Mounira");
    m_particulier = m_customers->insert(particulier).value();

    Customer pro;
    pro.name = QStringLiteral("Ali Jardins");
    pro.kind = CustomerKind::Professional;
    m_pro = m_customers->insert(pro).value();
}

void TestPos::proPricingSwitchesOnProCustomer()
{
    // Client de passage : prix particulier
    m_pos->addToCart(pick(m_variantId));
    QCOMPARE(m_pos->total(), Money::fromMillimes(25000));
    QVERIFY(!m_pos->proPricing());

    // Client professionnel : bascule au tarif pro (re-tarification, RG-04.c)
    m_pos->setCartCustomer(m_pro);
    QVERIFY(m_pos->proPricing());
    QCOMPARE(m_pos->total(), Money::fromMillimes(20000));

    // Ajout après bascule : utilise directement le tarif pro
    m_pos->addToCart(pick(m_variantId));
    QCOMPARE(m_pos->total(), Money::fromMillimes(40000)); // 2 × 20,000

    // Client particulier : retour au tarif normal
    m_pos->setCartCustomer(m_particulier);
    QVERIFY(!m_pos->proPricing());
    QCOMPARE(m_pos->total(), Money::fromMillimes(50000)); // 2 × 25,000

    // Retrait du client : tarif normal conservé
    m_pos->setCartCustomer(0);
    QCOMPARE(m_pos->cartCustomerId(), 0);
    QCOMPARE(m_pos->total(), Money::fromMillimes(50000));
}

void TestPos::negotiatedLinePriceSurvivesReprice()
{
    m_pos->clearCart();

    // Prix affiché 25,000 ; le vendeur négocie à 22,000 (F04-01)
    m_pos->addToCart(pick(m_variantId));
    QVERIFY(m_pos->setLinePrice(0, QStringLiteral("22,000")));
    QCOMPARE(m_pos->total(), Money::fromMillimes(22000));

    // Passer un client pro NE doit PAS écraser le prix négocié
    m_pos->setCartCustomer(m_pro);
    QVERIFY(m_pos->proPricing());
    QCOMPARE(m_pos->total(), Money::fromMillimes(22000));

    // Retour particulier : le prix négocié tient toujours
    m_pos->setCartCustomer(0);
    QCOMPARE(m_pos->total(), Money::fromMillimes(22000));

    // Prix négatif refusé
    QVERIFY(!m_pos->setLinePrice(0, QStringLiteral("-5")));
}

void TestPos::holdAndResumeCart()
{
    m_pos->clearCart();
    QCOMPARE(m_pos->heldCarts().size(), 0);

    // Panier A : 1 article (25,000), mis en attente
    m_pos->addToCart(pick(m_variantId));
    m_pos->holdCart();
    QCOMPARE(m_pos->heldCarts().size(), 1);
    QCOMPARE(m_pos->itemCount(), 0); // panier courant vidé

    // Panier B : 2 articles, puis reprise de A -> B auto-mis en attente
    m_pos->addToCart(pick(m_variantId));
    m_pos->addToCart(pick(m_variantId));
    QCOMPARE(m_pos->itemCount(), 2);
    m_pos->resumeCart(0);

    // A restauré (1 article, 25,000) ; B désormais en attente
    QCOMPARE(m_pos->itemCount(), 1);
    QCOMPARE(m_pos->total(), Money::fromMillimes(25000));
    QCOMPARE(m_pos->heldCarts().size(), 1);
    QCOMPARE(m_pos->heldCarts().first().toMap()
                 .value(QStringLiteral("itemCount")).toInt(), 2);
}

void TestPos::loadAcceptedQuoteIntoCart()
{
    // Devis : 2 citronniers à prix négocié 22,000 + une prestation libre.
    SqliteQuoteRepository quotes(m_db->connectionName());
    m_pos->configureQuotes(&quotes);

    QuoteLine product;
    product.variantId = m_variantId;
    product.label = QStringLiteral("Citronnier — pot21");
    product.qty = 2;
    product.unitPrice = Money::fromMillimes(22000);
    QuoteLine service;
    service.label = QStringLiteral("Plantation sur site");
    service.qty = 1;
    service.unitPrice = Money::fromMillimes(80000);
    QuoteDraft draft;
    draft.customerId = m_pro;
    draft.lines = {product, service};
    const int quoteId = quotes.create(draft).value().id;

    // Non accepté -> refus de chargement.
    QVERIFY(!m_pos->loadQuote(quoteId));

    QVERIFY(quotes.setStatus(quoteId, QuoteStatus::Accepted).isOk());

    // Le panier courant (1 article du test précédent) part en attente.
    const int heldBefore = m_pos->heldCarts().size();
    QVERIFY(m_pos->loadQuote(quoteId));
    QCOMPARE(m_pos->heldCarts().size(), heldBefore + 1);

    // 3 articles (2 citronniers + 1 prestation), total = 2×22,000 + 80,000
    // = 124,000 ; le client pro est rattaché SANS écraser les prix du
    // devis (manualPrice).
    QCOMPARE(m_pos->itemCount(), 3);
    QCOMPARE(m_pos->total(), Money::fromMillimes(124000));
    QVERIFY(m_pos->proPricing());

    // Encaissement réel : la prestation ne touche pas le stock, le
    // citronnier si (2 sorties).
    QSqlQuery before(m_db->database());
    before.exec(QStringLiteral(
        "SELECT COALESCE(SUM(qty), 0) FROM stock WHERE variant_id = %1")
                    .arg(m_variantId));
    before.next();
    const int stockBefore = before.value(0).toInt();

    QVERIFY(m_pos->checkout(QVariantMap{
        {QStringLiteral("method"), QStringLiteral("cash")},
    }));

    QSqlQuery after(m_db->database());
    after.exec(QStringLiteral(
        "SELECT COALESCE(SUM(qty), 0) FROM stock WHERE variant_id = %1")
                   .arg(m_variantId));
    after.next();
    QCOMPARE(after.value(0).toInt(), stockBefore - 2);

    // La ligne prestation est enregistrée avec variant NULL.
    QSqlQuery freeLine(m_db->database());
    freeLine.exec(QStringLiteral(
        "SELECT count(*) FROM sale_lines "
        "WHERE variant_id IS NULL AND label_snapshot = 'Plantation sur site'"));
    freeLine.next();
    QCOMPARE(freeLine.value(0).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestPos)
#include "tst_pos.moc"
