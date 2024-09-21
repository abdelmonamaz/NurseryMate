#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_batch_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_report_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestReports : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void summaryCountsAndTotal();
    void byCategoryRanked();
    void byPaymentSplit();
    void stockValuationAtCmp();
    void productionLossesByReason();
    void marginReport();
    void emptyRangeYieldsNothing();
    void netOfCreditNotes();

private:
    int addProduct(const QString& name, int categoryId, qint64 price,
                   qint64 avgCost);
    void sell(int variantId, const QString& label, qint64 price, int qty,
              PaymentMethod method);

    QString today() const { return QDate::currentDate().toString(Qt::ISODate); }

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    SqliteReportRepository* m_reports = nullptr;
    int m_vente = 0;
    int m_olivierProduct = 0;
};

int TestReports::addProduct(const QString& name, int categoryId, qint64 price,
                            qint64 avgCost)
{
    SqliteProductRepository products(m_db->connectionName());
    Product product;
    product.nameFr = name;
    product.categoryId = categoryId;
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(price);
    const int productId = products.insertWithVariants(product, {godet}).value();
    const int variantId = products.variantsOf(productId).value().first().id;

    // CMP établi directement (normalement mis à jour par une réception).
    QSqlQuery cost(m_db->database());
    cost.prepare(QStringLiteral("UPDATE variants SET avg_cost = ? WHERE id = ?"));
    cost.addBindValue(avgCost);
    cost.addBindValue(variantId);
    cost.exec();

    // Entrée de stock initiale (30 unités).
    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_vente;
    entry.variantId = variantId;
    entry.qty = 30;
    m_stock->recordMove(entry);

    if (name == QLatin1String("Olivier"))
        m_olivierProduct = productId;
    return variantId;
}

void TestReports::sell(int variantId, const QString& label, qint64 price, int qty,
                       PaymentMethod method)
{
    SaleLine line;
    line.variantId = variantId;
    line.label = label;
    line.qty = qty;
    line.unitPrice = Money::fromMillimes(price);
    SaleDraft draft;
    draft.lines = {line};
    draft.stockLocationId = m_vente;
    draft.method = method;
    QVERIFY(m_sales->record(draft).isOk());
}

void TestReports::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("rep.db")),
                               QStringLiteral("tst_reports"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);
    m_reports = new SqliteReportRepository(m_db->connectionName());

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id;

    // Olivier (cat. 1, prix 35,000 / CMP 20,000), Romarin (cat. 4, 3,500 / 2,000)
    const int olivier = addProduct(QStringLiteral("Olivier"), 1, 35000, 20000);
    const int romarin = addProduct(QStringLiteral("Romarin"), 4, 3500, 2000);

    // Deux ventes du jour : 2 oliviers en espèces (70,000),
    // 3 romarins par chèque (10,500).
    sell(olivier, QStringLiteral("Olivier — godet"), 35000, 2, PaymentMethod::Cash);
    sell(romarin, QStringLiteral("Romarin — godet"), 3500, 3, PaymentMethod::Cheque);

    // Lot de production d'oliviers avec pertes typées (F08-02).
    SqliteBatchRepository batches(m_db->connectionName(), *m_stock);
    BatchDraft draft;
    draft.productId = m_olivierProduct;
    draft.qtyInitial = 100;
    draft.locationId = m_vente;
    const auto batch = batches.create(draft);
    QVERIFY(batch.isOk());
    QVERIFY(batches.recordLoss(batch.value().id, 5,
                               QStringLiteral("mortality"), {}, 0).isOk());
    QVERIFY(batches.recordLoss(batch.value().id, 2,
                               QStringLiteral("frost"), {}, 0).isOk());
}

void TestReports::summaryCountsAndTotal()
{
    const auto s = m_reports->salesSummary(today(), today());
    QVERIFY2(s.isOk(), qPrintable(s.isOk() ? QString() : s.error().message));
    QCOMPARE(s.value().saleCount, 2);
    QCOMPARE(s.value().total, Money::fromMillimes(80500));
    // Panier moyen = 80,500 / 2 = 40,250
    QCOMPARE(s.value().averageBasket(), Money::fromMillimes(40250));
}

void TestReports::byCategoryRanked()
{
    const auto c = m_reports->salesByCategory(today(), today());
    QVERIFY(c.isOk());
    QCOMPARE(c.value().size(), 2);
    // Classement par CA décroissant : Arbres fruitiers (70,000) devant
    // Plantes méditerranéennes & aromatiques (10,500)
    QCOMPARE(c.value().at(0).label, QStringLiteral("Arbres fruitiers"));
    QCOMPARE(c.value().at(0).qty, 2);
    QCOMPARE(c.value().at(0).amount, Money::fromMillimes(70000));
    QCOMPARE(c.value().at(1).qty, 3);
    QCOMPARE(c.value().at(1).amount, Money::fromMillimes(10500));
}

void TestReports::byPaymentSplit()
{
    const auto p = m_reports->salesByPayment(today(), today());
    QVERIFY(p.isOk());
    QCOMPARE(p.value().size(), 2);
    // Espèces (70,000) devant Chèque (10,500)
    QCOMPARE(p.value().at(0).label, QStringLiteral("Espèces"));
    QCOMPARE(p.value().at(0).qty, 1);
    QCOMPARE(p.value().at(0).amount, Money::fromMillimes(70000));
    QCOMPARE(p.value().at(1).label, QStringLiteral("Chèque"));
    QCOMPARE(p.value().at(1).amount, Money::fromMillimes(10500));
}

void TestReports::stockValuationAtCmp()
{
    const auto v = m_reports->stockValuation();
    QVERIFY(v.isOk());
    QCOMPARE(v.value().size(), 2);
    // Olivier : 30 - 2 = 28 × 20,000 = 560,000 (valeur la plus haute d'abord)
    QCOMPARE(v.value().at(0).label, QStringLiteral("Olivier"));
    QCOMPARE(v.value().at(0).qty, 28);
    QCOMPARE(v.value().at(0).amount, Money::fromMillimes(560000));
    // Romarin : 30 - 3 = 27 × 2,000 = 54,000
    QCOMPARE(v.value().at(1).label, QStringLiteral("Romarin"));
    QCOMPARE(v.value().at(1).qty, 27);
    QCOMPARE(v.value().at(1).amount, Money::fromMillimes(54000));
}

void TestReports::productionLossesByReason()
{
    const auto l = m_reports->productionLosses(today(), today());
    QVERIFY(l.isOk());
    QCOMPARE(l.value().size(), 2);
    // Mortalité (5) devant Gel (2)
    QCOMPARE(l.value().at(0).label, QStringLiteral("Mortalité"));
    QCOMPARE(l.value().at(0).qty, 5);
    QCOMPARE(l.value().at(1).label, QStringLiteral("Gel"));
    QCOMPARE(l.value().at(1).qty, 2);
}

void TestReports::marginReport()
{
    // Par catégorie : Olivier (cat 1) CA 70,000, coût 2×20,000=40,000 ->
    // marge 30,000 (42,8 %) ; Romarin (cat 4) CA 10,500, coût 3×2,000=6,000
    // -> marge 4,500. Tri par marge décroissante.
    const auto byCategory = m_reports->margins(today(), today(), false);
    QVERIFY2(byCategory.isOk(),
             qPrintable(byCategory.isOk() ? QString()
                                          : byCategory.error().message));
    QCOMPARE(byCategory.value().size(), 2);
    QCOMPARE(byCategory.value().at(0).label, QStringLiteral("Arbres fruitiers"));
    QCOMPARE(byCategory.value().at(0).revenue, Money::fromMillimes(70000));
    QCOMPARE(byCategory.value().at(0).cost, Money::fromMillimes(40000));
    QCOMPARE(byCategory.value().at(0).margin(), Money::fromMillimes(30000));
    QCOMPARE(byCategory.value().at(0).marginPerMille(), 428);
    QCOMPARE(byCategory.value().at(1).margin(), Money::fromMillimes(4500));

    // Par produit : mêmes chiffres, libellés produits.
    const auto byProduct = m_reports->margins(today(), today(), true);
    QVERIFY(byProduct.isOk());
    QCOMPARE(byProduct.value().size(), 2);
    QCOMPARE(byProduct.value().at(0).label, QStringLiteral("Olivier"));
    QCOMPARE(byProduct.value().at(1).label, QStringLiteral("Romarin"));
}

void TestReports::emptyRangeYieldsNothing()
{
    // Une plage passée sans activité : synthèse à zéro, listes vides.
    const QString past = QStringLiteral("2000-01-01");
    const auto s = m_reports->salesSummary(past, past);
    QVERIFY(s.isOk());
    QCOMPARE(s.value().saleCount, 0);
    QCOMPARE(s.value().total, Money::fromMillimes(0));

    QVERIFY(m_reports->salesByCategory(past, past).value().isEmpty());
    QVERIFY(m_reports->productionLosses(past, past).value().isEmpty());
    // La valorisation du stock ignore la plage (photo courante).
    QCOMPARE(m_reports->stockValuation().value().size(), 2);
}

void TestReports::netOfCreditNotes()
{
    // Avoir total sur la vente d'oliviers (70,000) : le CA brut reste
    // 80,500 mais le CA net tombe à 10,500 — l'avoir compte à SA date.
    QSqlQuery firstSale(m_db->database());
    QVERIFY(firstSale.exec(QStringLiteral(
        "SELECT id FROM sales WHERE total = 70000 LIMIT 1")));
    QVERIFY(firstSale.next());

    CreditNoteDraft draft;
    draft.saleId = firstSale.value(0).toInt();
    draft.reason = QStringLiteral("client remboursé — plants non conformes");
    draft.restock = false; // ne pas toucher la valorisation déjà vérifiée
    draft.refundMethod = QStringLiteral("cash");
    QVERIFY(m_sales->createCreditNote(draft).isOk());

    const auto s = m_reports->salesSummary(today(), today());
    QVERIFY2(s.isOk(), qPrintable(s.isOk() ? QString() : s.error().message));
    QCOMPARE(s.value().saleCount, 2);
    QCOMPARE(s.value().total, Money::fromMillimes(80500));
    QCOMPARE(s.value().creditTotal, Money::fromMillimes(70000));
    QCOMPARE(s.value().net(), Money::fromMillimes(10500));
}

QTEST_GUILESS_MAIN(TestReports)
#include "tst_reports.moc"
