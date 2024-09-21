#include "database/database_manager.h"
#include "generators/invoice_generator.h"
#include "repositories/sqlite/sqlite_customer_repository.h"
#include "repositories/sqlite/sqlite_invoice_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_settings_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestInvoice : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createFromSaleComputesHtVat();
    void oneInvoicePerSale();
    void breakdownWithDiscount();
    void cancelledSaleRejected();
    void pdfGenerated();

private:
    int makeSale(qint64 unitTtc, int qty, int vatRate, qint64 discount = 0,
                 int customerId = 0);

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    SqliteInvoiceRepository* m_invoices = nullptr;
    int m_variantId = 0;
    int m_vente = 0;
    int m_saleId = 0;
};

int TestInvoice::makeSale(qint64 unitTtc, int qty, int vatRate, qint64 discount,
                          int customerId)
{
    SaleLine line;
    line.variantId = m_variantId;
    line.label = QStringLiteral("Olivier — pot21");
    line.qty = qty;
    line.unitPrice = Money::fromMillimes(unitTtc);
    line.vatRatePercent = vatRate;

    SaleDraft draft;
    draft.lines = {line};
    draft.globalDiscount = Money::fromMillimes(discount);
    draft.stockLocationId = m_vente;
    draft.customerId = customerId;
    return m_sales->record(draft).value().id;
}

void TestInvoice::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("inv.db")),
                               QStringLiteral("tst_invoice"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);
    m_invoices = new SqliteInvoiceRepository(m_db->connectionName());

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id;

    SqliteProductRepository products(m_db->connectionName());
    Product olivier;
    olivier.nameFr = QStringLiteral("Olivier");
    Variant pot21;
    pot21.packaging = QStringLiteral("pot21");
    pot21.priceTtc = Money::fromMillimes(35000);
    pot21.vatRatePercent = 19;
    const int productId = products.insertWithVariants(olivier, {pot21}).value();
    m_variantId = products.variantsOf(productId).value().first().id;
}

void TestInvoice::createFromSaleComputesHtVat()
{
    // 1 × 11,900 TTC à 19 % : HT 10,000, TVA 1,900. Timbre 1,000.
    m_saleId = makeSale(11900, 1, 19);
    const auto invoice = m_invoices->createFromSale(
        m_saleId, Money::fromMillimes(1000), 0);
    QVERIFY2(invoice.isOk(),
             qPrintable(invoice.isOk() ? QString() : invoice.error().message));

    QVERIFY(invoice.value().number.startsWith(QStringLiteral("F-")));
    QVERIFY(invoice.value().number.endsWith(QStringLiteral("00001")));
    QCOMPARE(invoice.value().subtotalHt, Money::fromMillimes(10000));
    QCOMPARE(invoice.value().vatTotal, Money::fromMillimes(1900));
    QCOMPARE(invoice.value().stampDuty, Money::fromMillimes(1000));
    QCOMPARE(invoice.value().total, Money::fromMillimes(12900));
}

void TestInvoice::oneInvoicePerSale()
{
    // Re-facturer la même vente retourne la facture existante (même n°)
    const auto again = m_invoices->createFromSale(
        m_saleId, Money::fromMillimes(1000), 0);
    QVERIFY(again.isOk());
    QVERIFY(again.value().number.endsWith(QStringLiteral("00001")));
    QCOMPARE(m_invoices->invoiceIdForSale(m_saleId).value(), again.value().id);

    // Une nouvelle vente -> facture n° 2
    const int otherSale = makeSale(5000, 1, 0);
    const auto other = m_invoices->createFromSale(
        otherSale, Money::fromMillimes(1000), 0);
    QVERIFY(other.value().number.endsWith(QStringLiteral("00002")));
}

void TestInvoice::breakdownWithDiscount()
{
    // 2 × 11,900 = 23,800 TTC, remise 3,800 -> net 20,000 TTC à 19 %
    // HT = 20000/1.19 ≈ 16,807 ; TVA ≈ 3,193 ; total sale 20,000
    const int saleId = makeSale(11900, 2, 19, 3800);
    const auto invoice = m_invoices->createFromSale(
        saleId, Money::fromMillimes(1000), 0);
    QVERIFY(invoice.isOk());

    const auto details = m_invoices->details(invoice.value().id);
    QVERIFY(details.isOk());
    QCOMPARE(details.value().vatRows.size(), 1);
    QCOMPARE(details.value().vatRows.first().ratePercent, 19);
    // Base HT + TVA = net TTC (20,000) ; reconciliation exacte
    const Money reconciled = details.value().vatRows.first().baseHt
        + details.value().vatRows.first().vat;
    QCOMPARE(reconciled, Money::fromMillimes(20000));
    QCOMPARE(details.value().header.total, Money::fromMillimes(21000)); // +timbre
}

void TestInvoice::cancelledSaleRejected()
{
    const int saleId = makeSale(5000, 1, 0);
    QVERIFY(m_sales->cancel(saleId, QStringLiteral("test"), 0).isOk());
    QVERIFY(!m_invoices->createFromSale(saleId, Money::fromMillimes(1000), 0)
                 .isOk());
}

void TestInvoice::pdfGenerated()
{
    SqliteSettingsRepository settings(m_db->connectionName());
    const auto details = m_invoices->details(
        m_invoices->invoiceIdForSale(m_saleId).value());
    QVERIFY(details.isOk());

    const auto pdf = InvoiceGenerator::generatePdf(
        details.value(), InvoiceGenerator::companyFrom(settings),
        m_dir.filePath(QStringLiteral("invoices")));
    QVERIFY2(pdf.isOk(), qPrintable(pdf.isOk() ? QString() : pdf.error().message));

    const QFileInfo file(pdf.value());
    QVERIFY(file.exists());
    QVERIFY(file.size() > 1000);
    QCOMPARE(file.completeBaseName(), details.value().header.number);
}

QTEST_MAIN(TestInvoice)
#include "tst_invoice.moc"
