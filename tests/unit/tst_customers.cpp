#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_customer_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestCustomers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createAndSearch();
    void updateAndDeactivate();
    void phoneDuplicateDetected();
    void creditSaleFeedsBalance();
    void creditRequiresCustomer();
    void creditLimitEnforced();
    void paymentReducesBalance();
    void cashSaleWithCustomerLeavesNoDebt();
    void deleteOnlyIfNeverReferenced();

private:
    SaleDraft draftFor(int customerId, qint64 totalMillimes, bool credit) const;

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteCustomerRepository* m_customers = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    int m_ali = 0;      // paysagiste, plafond 200,000
    int m_variantId = 0;
    int m_vente = 0;
};

SaleDraft TestCustomers::draftFor(int customerId, qint64 totalMillimes,
                                  bool credit) const
{
    SaleLine line;
    line.variantId = m_variantId;
    line.label = QStringLiteral("Olivier — pot21");
    line.qty = 1;
    line.unitPrice = Money::fromMillimes(totalMillimes);

    SaleDraft draft;
    draft.lines = {line};
    draft.stockLocationId = m_vente;
    draft.customerId = customerId;
    draft.onCredit = credit;
    return draft;
}

void TestCustomers::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("cust.db")),
                               QStringLiteral("tst_customers"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_customers = new SqliteCustomerRepository(m_db->connectionName());
    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id;

    SqliteProductRepository products(m_db->connectionName());
    Product olivier;
    olivier.nameFr = QStringLiteral("Olivier");
    Variant pot21;
    pot21.packaging = QStringLiteral("pot21");
    pot21.priceTtc = Money::fromMillimes(35000);
    const int productId = products.insertWithVariants(olivier, {pot21}).value();
    m_variantId = products.variantsOf(productId).value().first().id;
}

void TestCustomers::createAndSearch()
{
    Customer ali;
    ali.name = QStringLiteral("Ali Jardins");
    ali.kind = CustomerKind::Professional;
    ali.phone = QStringLiteral("22123456");
    ali.creditLimitMillimes = 200000;
    const auto created = m_customers->insert(ali);
    QVERIFY(created.isOk());
    m_ali = created.value();

    Customer mounira;
    mounira.name = QStringLiteral("Mounira");
    QVERIFY(m_customers->insert(mounira).isOk());

    // Recherche par nom et par téléphone (F06-04)
    QCOMPARE(m_customers->search(QStringLiteral("ali")).value().size(), 1);
    QCOMPARE(m_customers->search(QStringLiteral("2212")).value().size(), 1);
    QCOMPARE(m_customers->search(QString()).value().size(), 2);

    const CustomerRow row =
        m_customers->search(QStringLiteral("ali")).value().first();
    QCOMPARE(row.kind, CustomerKind::Professional);
    QCOMPARE(row.balance, Money::fromMillimes(0));
    QCOMPARE(row.creditLimitMillimes, 200000);
}

void TestCustomers::updateAndDeactivate()
{
    // Édition de fiche : aller-retour complet (téléphone corrigé, adresse).
    Customer ali = m_customers->byId(m_ali).value();
    ali.phone = QStringLiteral("22999888");
    ali.address = QStringLiteral("Route de Tunis, Sfax");
    ali.notes = QStringLiteral("préfère livraison le matin");
    QVERIFY(m_customers->update(ali).isOk());

    Customer reread = m_customers->byId(m_ali).value();
    QCOMPARE(reread.phone, QStringLiteral("22999888"));
    QCOMPARE(reread.address, QStringLiteral("Route de Tunis, Sfax"));
    QCOMPARE(reread.notes, QStringLiteral("préfère livraison le matin"));

    // Restaure le numéro d'origine (utilisé par les tests suivants).
    reread.phone = QStringLiteral("22123456");
    QVERIFY(m_customers->update(reread).isOk());

    // Désactivation (RG-06.a) : disparaît de la recherche par défaut,
    // reste visible avec includeInactive, jamais supprimé.
    Customer jetable;
    jetable.name = QStringLiteral("Client Test");
    const int id = m_customers->insert(jetable).value();
    QVERIFY(m_customers->setActive(id, false).isOk());
    QCOMPARE(m_customers->search(QStringLiteral("Client Test")).value().size(), 0);
    QCOMPARE(m_customers->search(QStringLiteral("Client Test"), true)
                 .value().size(), 1);
    QVERIFY(m_customers->byId(id).isOk());
}

void TestCustomers::phoneDuplicateDetected()
{
    QVERIFY(m_customers->phoneExists(QStringLiteral("22123456")).value());
    QVERIFY(!m_customers->phoneExists(QStringLiteral("99999999")).value());
    QVERIFY(!m_customers->phoneExists(QString()).value());
}

void TestCustomers::creditSaleFeedsBalance()
{
    // Vente à crédit 35,000 : pas de paiement, encours = 35,000 (F04-05)
    const auto sale = m_sales->record(draftFor(m_ali, 35000, true));
    QVERIFY2(sale.isOk(),
             qPrintable(sale.isOk() ? QString() : sale.error().message));

    QCOMPARE(m_customers->balanceOf(m_ali).value(), Money::fromMillimes(35000));

    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT paid_total, customer_id FROM sales WHERE id = %1")
                           .arg(sale.value().id)));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), 0);
    QCOMPARE(query.value(1).toInt(), m_ali);
}

void TestCustomers::creditRequiresCustomer()
{
    QVERIFY(!m_sales->record(draftFor(0, 10000, true)).isOk());
}

void TestCustomers::creditLimitEnforced()
{
    // Encours 35,000 + vente 180,000 > plafond 200,000 -> refus (RG-06.b)
    const auto blocked = m_sales->record(draftFor(m_ali, 180000, true));
    QVERIFY(!blocked.isOk());
    QCOMPARE(blocked.error().code, QStringLiteral("sale.creditLimit"));

    // 150,000 passe (35,000 + 150,000 <= 200,000)
    QVERIFY(m_sales->record(draftFor(m_ali, 150000, true)).isOk());
    QCOMPARE(m_customers->balanceOf(m_ali).value(), Money::fromMillimes(185000));
}

void TestCustomers::paymentReducesBalance()
{
    // Règlement sur encours 85,000 (F06-03)
    QVERIFY(m_customers->recordPayment(m_ali, Money::fromMillimes(85000),
                                       PaymentMethod::Cash, {}, {}, 0)
                .isOk());
    QCOMPARE(m_customers->balanceOf(m_ali).value(), Money::fromMillimes(100000));

    // Montant nul refusé
    QVERIFY(!m_customers->recordPayment(m_ali, Money::fromMillimes(0),
                                        PaymentMethod::Cash, {}, {}, 0)
                 .isOk());
}

void TestCustomers::cashSaleWithCustomerLeavesNoDebt()
{
    // Vente comptant rattachée au client : encours inchangé (paiement lié)
    QVERIFY(m_sales->record(draftFor(m_ali, 20000, false)).isOk());
    QCOMPARE(m_customers->balanceOf(m_ali).value(), Money::fromMillimes(100000));
}

void TestCustomers::deleteOnlyIfNeverReferenced()
{
    // Ajout fautif jamais utilisé : suppression physique admise (norme).
    Customer fautif;
    fautif.name = QStringLiteral("Doublon Saisie");
    const int id = m_customers->insert(fautif).value();
    QVERIFY(!m_customers->isReferenced(id).value());
    QVERIFY(m_customers->remove(id).isOk());
    QVERIFY(!m_customers->byId(id).isOk());

    // Ali a des ventes : suppression refusée, fiche intacte.
    QVERIFY(m_customers->isReferenced(m_ali).value());
    const auto refused = m_customers->remove(m_ali);
    QVERIFY(!refused.isOk());
    QCOMPARE(refused.error().code, QStringLiteral("customer.referenced"));
    QVERIFY(m_customers->byId(m_ali).isOk());
}

QTEST_GUILESS_MAIN(TestCustomers)
#include "tst_customers.moc"
