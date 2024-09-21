#include "controllers/report_controller.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_report_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_settings_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

using namespace nursera;

// QTEST_MAIN obligatoire : QTextDocument -> PDF exige QGuiApplication.
class TestReportPdf : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pdfForSalesCategory();
    void pdfForStockValuation();
    void unknownReportYieldsEmpty();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteReportRepository* m_reports = nullptr;
    SqliteSettingsRepository* m_settings = nullptr;
    ReportController* m_controller = nullptr;
};

void TestReportPdf::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("rpdf.db")),
                               QStringLiteral("tst_report_pdf"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_reports = new SqliteReportRepository(m_db->connectionName());
    m_settings = new SqliteSettingsRepository(m_db->connectionName());
    m_controller = new ReportController(*m_reports, *m_settings,
                                        m_dir.filePath(QStringLiteral("exports")));

    // Un produit vendu aujourd'hui pour peupler le rapport catégorie.
    SqliteLocationRepository locations(m_db->connectionName());
    const int vente = locations.all().value().at(1).id;

    SqliteProductRepository products(m_db->connectionName());
    Product product;
    product.nameFr = QStringLiteral("Olivier");
    product.categoryId = 1;
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(35000);
    const int productId = products.insertWithVariants(product, {godet}).value();
    const int variantId = products.variantsOf(productId).value().first().id;

    QSqlQuery cost(m_db->database());
    cost.prepare(QStringLiteral("UPDATE variants SET avg_cost = 20000 WHERE id = ?"));
    cost.addBindValue(variantId);
    QVERIFY(cost.exec());

    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = vente;
    entry.variantId = variantId;
    entry.qty = 10;
    QVERIFY(m_stock->recordMove(entry).isOk());

    SqliteSaleRepository sales(m_db->connectionName(), *m_stock);
    SaleLine line;
    line.variantId = variantId;
    line.label = QStringLiteral("Olivier — godet");
    line.qty = 2;
    line.unitPrice = Money::fromMillimes(35000);
    SaleDraft draft;
    draft.lines = {line};
    draft.stockLocationId = vente;
    QVERIFY(sales.record(draft).isOk());
}

void TestReportPdf::pdfForSalesCategory()
{
    const QString url = m_controller->exportPdf(QStringLiteral("sales_category"));
    QVERIFY(!url.isEmpty());
    const QString path = QUrl(url).toLocalFile();
    QVERIFY(path.endsWith(QStringLiteral(".pdf")));

    QFile pdf(path);
    QVERIFY(pdf.exists());
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    // Un PDF valide commence par %PDF et pèse plus qu'un en-tête vide.
    QCOMPARE(pdf.read(4), QByteArrayLiteral("%PDF"));
    QVERIFY(pdf.size() > 2000);
}

void TestReportPdf::pdfForStockValuation()
{
    const QString url = m_controller->exportPdf(QStringLiteral("stock"));
    QVERIFY(!url.isEmpty());
    QFile pdf(QUrl(url).toLocalFile());
    QVERIFY(pdf.exists());
    QVERIFY(pdf.open(QIODevice::ReadOnly));
    QCOMPARE(pdf.read(4), QByteArrayLiteral("%PDF"));
}

void TestReportPdf::unknownReportYieldsEmpty()
{
    QCOMPARE(m_controller->exportPdf(QStringLiteral("nope")), QString());
}

QTEST_MAIN(TestReportPdf)
#include "tst_report_pdf.moc"
