#include "database/database_manager.h"
#include "generators/credit_note_generator.h"
#include "generators/ticket_generator.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_settings_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestTicket : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void detailsRoundTrip();
    void companyFromSettings();
    void pdfGenerated();
    void creditNotePdfGenerated();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    int m_saleId = 0;
};

void TestTicket::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("ticket.db")),
                               QStringLiteral("tst_ticket"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    const int vente = locations.all().value().at(1).id;

    SqliteProductRepository products(m_db->connectionName());
    Product citronnier;
    citronnier.nameFr = QStringLiteral("Citronnier 4 saisons");
    Variant pot21;
    pot21.packaging = QStringLiteral("pot21");
    pot21.priceTtc = Money::fromMillimes(25000);
    pot21.vatRatePercent = 19;
    const int productId =
        products.insertWithVariants(citronnier, {pot21}).value();
    const int variantId = products.variantsOf(productId).value().first().id;

    SaleLine line;
    line.variantId = variantId;
    line.label = QStringLiteral("Citronnier 4 saisons — pot21");
    line.qty = 2;
    line.unitPrice = Money::fromMillimes(25000);
    line.vatRatePercent = 19;

    SaleDraft draft;
    draft.lines = {line};
    draft.globalDiscount = Money::fromMillimes(3000);
    draft.stockLocationId = vente;
    const auto sale = m_sales->record(draft);
    QVERIFY(sale.isOk());
    m_saleId = sale.value().id;
}

void TestTicket::detailsRoundTrip()
{
    const auto details = m_sales->details(m_saleId);
    QVERIFY2(details.isOk(),
             qPrintable(details.isOk() ? QString() : details.error().message));

    const SaleDetails& sale = details.value();
    QVERIFY(sale.number.endsWith(QStringLiteral("00001")));
    QCOMPARE(sale.lines.size(), 1);
    QCOMPARE(sale.lines.first().qty, 2);
    QCOMPARE(sale.lines.first().lineTotal, Money::fromMillimes(50000));
    QCOMPARE(sale.subtotal, Money::fromMillimes(50000));
    QCOMPARE(sale.discount, Money::fromMillimes(3000));
    QCOMPARE(sale.total, Money::fromMillimes(47000));
    QCOMPARE(sale.method, QStringLiteral("cash"));

    // Vente inexistante
    QVERIFY(!m_sales->details(99999).isOk());
}

void TestTicket::companyFromSettings()
{
    SqliteSettingsRepository settings(m_db->connectionName());

    // Replis par défaut
    auto company = TicketGenerator::companyFrom(settings);
    QCOMPARE(company.name, QStringLiteral("Pépinière Idéale"));
    QVERIFY(company.address.isEmpty());

    // Valeurs personnalisées (F11-01)
    QVERIFY(settings.setValue(QStringLiteral("company.address"),
                              QStringLiteral("Route de Tunis, Sfax")).isOk());
    QVERIFY(settings.setValue(QStringLiteral("company.phone"),
                              QStringLiteral("74 123 456")).isOk());
    company = TicketGenerator::companyFrom(settings);
    QCOMPARE(company.address, QStringLiteral("Route de Tunis, Sfax"));
    QCOMPARE(company.phone, QStringLiteral("74 123 456"));
}

void TestTicket::pdfGenerated()
{
    SqliteSettingsRepository settings(m_db->connectionName());
    const auto details = m_sales->details(m_saleId);
    QVERIFY(details.isOk());

    const auto pdf = TicketGenerator::generatePdf(
        details.value(), TicketGenerator::companyFrom(settings),
        m_dir.filePath(QStringLiteral("tickets")));
    QVERIFY2(pdf.isOk(), qPrintable(pdf.isOk() ? QString() : pdf.error().message));

    const QFileInfo file(pdf.value());
    QVERIFY(file.exists());
    QVERIFY2(file.size() > 1000, qPrintable(QString::number(file.size())));
    QVERIFY(file.fileName().endsWith(QStringLiteral(".pdf")));
    QCOMPARE(file.completeBaseName(), details.value().number);
}

void TestTicket::creditNotePdfGenerated()
{
    // Avoir sur la vente du ticket, puis PDF AV-AAAA-NNN.pdf.
    CreditNoteDraft draft;
    draft.saleId = m_saleId;
    draft.reason = QStringLiteral("plants non conformes au retour client");
    draft.restock = false;
    draft.refundMethod = QStringLiteral("cash");
    QVERIFY(m_sales->createCreditNote(draft).isOk());

    const auto note = m_sales->creditNoteDetails(m_saleId);
    QVERIFY2(note.isOk(),
             qPrintable(note.isOk() ? QString() : note.error().message));
    QCOMPARE(note.value().total, Money::fromMillimes(47000));

    SqliteSettingsRepository settings(m_db->connectionName());
    const auto pdf = CreditNoteGenerator::generatePdf(
        note.value(), InvoiceGenerator::companyFrom(settings),
        m_dir.filePath(QStringLiteral("avoirs")));
    QVERIFY2(pdf.isOk(), qPrintable(pdf.isOk() ? QString() : pdf.error().message));

    const QFileInfo file(pdf.value());
    QVERIFY(file.exists());
    QVERIFY2(file.size() > 1000, qPrintable(QString::number(file.size())));
    QCOMPARE(file.completeBaseName(), note.value().number);
}

// QTEST_MAIN : le rendu QTextDocument -> PDF exige une QGuiApplication
// (base de données de polices), pas une simple QCoreApplication.
QTEST_MAIN(TestTicket)
#include "tst_ticket.moc"
