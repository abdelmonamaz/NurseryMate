#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_dashboard_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestDashboard : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void kpisAggregateSales();
    void topProductsRanked();
    void lowStockAlerts();
    void salesTrendFillsGaps();
    void netOfCreditNotes();

private:
    int addProduct(const QString& name, qint64 price, int threshold = -1);

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    SqliteDashboardRepository* m_dashboard = nullptr;
    int m_vente = 0;
};

int TestDashboard::addProduct(const QString& name, qint64 price, int threshold)
{
    SqliteProductRepository products(m_db->connectionName());
    Product product;
    product.nameFr = name;
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(price);
    godet.alertThreshold = threshold;
    const int productId = products.insertWithVariants(product, {godet}).value();
    return products.variantsOf(productId).value().first().id;
}

void TestDashboard::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("dash.db")),
                               QStringLiteral("tst_dashboard"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);
    m_dashboard = new SqliteDashboardRepository(m_db->connectionName());

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id;

    // Romarin (3,500 — seuil 10) : 12 en stock ; Olivier (35,000) : 2 en stock
    const int romarin = addProduct(QStringLiteral("Romarin"), 3500, 10);
    const int olivier = addProduct(QStringLiteral("Olivier"), 35000);

    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_vente;
    entry.variantId = romarin;
    entry.qty = 12;
    QVERIFY(m_stock->recordMove(entry).isOk());

    // Deux ventes du jour : 4 romarins (14,000) puis 3 oliviers (105,000)
    auto sell = [this](int variantId, const QString& label, qint64 price, int qty) {
        SaleLine line;
        line.variantId = variantId;
        line.label = label;
        line.qty = qty;
        line.unitPrice = Money::fromMillimes(price);
        SaleDraft draft;
        draft.lines = {line};
        draft.stockLocationId = m_vente;
        QVERIFY(m_sales->record(draft).isOk());
    };
    sell(romarin, QStringLiteral("Romarin — godet"), 3500, 4);
    sell(olivier, QStringLiteral("Olivier — godet"), 35000, 3);
}

void TestDashboard::kpisAggregateSales()
{
    const auto kpis = m_dashboard->kpis();
    QVERIFY2(kpis.isOk(),
             qPrintable(kpis.isOk() ? QString() : kpis.error().message));

    QCOMPARE(kpis.value().todaySaleCount, 2);
    QCOMPARE(kpis.value().todayTotal, Money::fromMillimes(119000));
    // Aujourd'hui ⊆ 7 jours ⊆ mois
    QCOMPARE(kpis.value().weekTotal, Money::fromMillimes(119000));
    QCOMPARE(kpis.value().monthTotal, Money::fromMillimes(119000));
    // Panier moyen = 119,000 / 2 = 59,500
    QCOMPARE(kpis.value().averageBasket(), Money::fromMillimes(59500));
}

void TestDashboard::topProductsRanked()
{
    const auto top = m_dashboard->topProducts();
    QVERIFY(top.isOk());
    QCOMPARE(top.value().size(), 2);
    // Classement par chiffre d'affaires : Olivier (105,000) devant Romarin (14,000)
    QCOMPARE(top.value().at(0).label, QStringLiteral("Olivier — godet"));
    QCOMPARE(top.value().at(0).qtySold, 3);
    QCOMPARE(top.value().at(0).revenue, Money::fromMillimes(105000));
    QCOMPARE(top.value().at(1).qtySold, 4);
}

void TestDashboard::lowStockAlerts()
{
    const auto low = m_dashboard->lowStock();
    QVERIFY(low.isOk());
    QCOMPARE(low.value().size(), 2);

    // Pires d'abord : Olivier vendu sans stock -> -3 (négatif),
    // puis Romarin 12-4=8 <= seuil 10 (stock bas)
    QCOMPARE(low.value().at(0).productFr, QStringLiteral("Olivier"));
    QCOMPARE(low.value().at(0).qty, -3);
    QVERIFY(low.value().at(0).isNegative());

    QCOMPARE(low.value().at(1).productFr, QStringLiteral("Romarin"));
    QCOMPARE(low.value().at(1).qty, 8);
    QCOMPARE(low.value().at(1).alertThreshold, 10);
    QVERIFY(!low.value().at(1).isNegative());
}

void TestDashboard::salesTrendFillsGaps()
{
    // 14 points quotidiens, chronologiques, trous comblés à 0 (F10-01)
    const auto daily = m_dashboard->salesDaily(14);
    QVERIFY(daily.isOk());
    QCOMPARE(daily.value().size(), 14);

    // Toutes les ventes du test sont d'aujourd'hui -> dernier point = 119,000,
    // les jours précédents = 0
    QCOMPARE(daily.value().last().total, Money::fromMillimes(119000));
    QCOMPARE(daily.value().first().total, Money::fromMillimes(0));

    // 6 points mensuels, le mois courant porte le total
    const auto monthly = m_dashboard->salesMonthly(6);
    QVERIFY(monthly.isOk());
    QCOMPARE(monthly.value().size(), 6);
    QCOMPARE(monthly.value().last().total, Money::fromMillimes(119000));
}

void TestDashboard::netOfCreditNotes()
{
    // Avoir total sur la vente de romarins (14,000) : tous les CA du
    // dashboard deviennent nets — KPI et séries quotidienne/mensuelle.
    QSqlQuery firstSale(m_db->database());
    QVERIFY(firstSale.exec(QStringLiteral(
        "SELECT id FROM sales WHERE total = 14000 LIMIT 1")));
    QVERIFY(firstSale.next());

    CreditNoteDraft draft;
    draft.saleId = firstSale.value(0).toInt();
    draft.reason = QStringLiteral("retour client");
    draft.restock = false;
    draft.refundMethod = QStringLiteral("cash");
    QVERIFY(m_sales->createCreditNote(draft).isOk());

    const auto kpis = m_dashboard->kpis();
    QVERIFY(kpis.isOk());
    QCOMPARE(kpis.value().todayTotal, Money::fromMillimes(105000));
    QCOMPARE(kpis.value().weekTotal, Money::fromMillimes(105000));
    QCOMPARE(kpis.value().monthTotal, Money::fromMillimes(105000));

    const auto daily = m_dashboard->salesDaily(14);
    QVERIFY(daily.isOk());
    QCOMPARE(daily.value().last().total, Money::fromMillimes(105000));
    const auto monthly = m_dashboard->salesMonthly(6);
    QVERIFY(monthly.isOk());
    QCOMPARE(monthly.value().last().total, Money::fromMillimes(105000));
}

QTEST_GUILESS_MAIN(TestDashboard)
#include "tst_dashboard.moc"
