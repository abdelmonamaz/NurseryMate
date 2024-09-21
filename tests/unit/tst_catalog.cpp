#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_category_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestCatalog : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void categoriesSeeded();
    void createAndSearchProduct();
    void searchArabic();
    void searchBotanical();
    void searchNoMatch();
    void variantsRoundTrip();
    void deactivateHidesFromSearch();
    void updateProductFields();
    void addAndUpdateVariants();
    void searchByBarcode();
    void barcodeDuplicateDetected();
    void mainPhotoRoundTrip();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    int m_productId = 0;
};

void TestCatalog::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("catalog.db")),
                               QStringLiteral("tst_catalog"));
    const auto opened = m_db->open();
    QVERIFY2(opened.isOk(),
             qPrintable(opened.isOk() ? QString() : opened.error().message));
    QCOMPARE(m_db->schemaVersion(), 20);
}

void TestCatalog::categoriesSeeded()
{
    SqliteCategoryRepository categories(m_db->connectionName());
    const auto all = categories.all();
    QVERIFY(all.isOk());
    QCOMPARE(all.value().size(), 15); // 10 (migration 002) + 5 jardinerie (009)
    QCOMPARE(all.value().first().nameFr, QStringLiteral("Arbres fruitiers"));
    QCOMPARE(all.value().first().nameAr, QStringLiteral("أشجار مثمرة"));
}

void TestCatalog::createAndSearchProduct()
{
    SqliteCategoryRepository categories(m_db->connectionName());
    SqliteProductRepository products(m_db->connectionName());

    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    romarin.nameAr = QStringLiteral("إكليل الجبل");
    romarin.botanicalName = QStringLiteral("Rosmarinus officinalis");
    romarin.categoryId = categories.all().value().at(3).id; // Méditerranéennes

    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    Variant pot14;
    pot14.packaging = QStringLiteral("pot14");
    pot14.priceTtc = Money::fromMillimes(8000);

    const auto created = products.insertWithVariants(romarin, {godet, pot14});
    QVERIFY2(created.isOk(),
             qPrintable(created.isOk() ? QString() : created.error().message));
    m_productId = created.value();
    QVERIFY(m_productId > 0);

    // Recherche FR partielle, insensible à la casse (F02-06)
    const auto found = products.search(QStringLiteral("roma"));
    QVERIFY(found.isOk());
    QCOMPARE(found.value().size(), 1);
    const ProductRow& row = found.value().first();
    QCOMPARE(row.id, m_productId);
    QCOMPARE(row.variantCount, 2);
    QCOMPARE(row.minPriceTtc, Money::fromMillimes(3500));
    QVERIFY(!row.categoryFr.isEmpty());
}

void TestCatalog::searchArabic()
{
    SqliteProductRepository products(m_db->connectionName());
    const auto found = products.search(QStringLiteral("إكليل"));
    QVERIFY(found.isOk());
    QCOMPARE(found.value().size(), 1);
    QCOMPARE(found.value().first().nameAr, QStringLiteral("إكليل الجبل"));
}

void TestCatalog::searchBotanical()
{
    SqliteProductRepository products(m_db->connectionName());
    const auto found = products.search(QStringLiteral("officinalis"));
    QVERIFY(found.isOk());
    QCOMPARE(found.value().size(), 1);
}

void TestCatalog::searchNoMatch()
{
    SqliteProductRepository products(m_db->connectionName());
    const auto found = products.search(QStringLiteral("tomate"));
    QVERIFY(found.isOk());
    QCOMPARE(found.value().size(), 0);
}

void TestCatalog::variantsRoundTrip()
{
    SqliteProductRepository products(m_db->connectionName());
    const auto variants = products.variantsOf(m_productId);
    QVERIFY(variants.isOk());
    QCOMPARE(variants.value().size(), 2);

    const Variant& godet = variants.value().first(); // tri par packaging
    QCOMPARE(godet.packaging, QStringLiteral("godet"));
    QCOMPARE(godet.priceTtc, Money::fromMillimes(3500));
    QCOMPARE(godet.alertThreshold, -1);
    // SKU généré PNNNN-CONDITIONNEMENT (RG-02.b)
    QVERIFY2(godet.sku.startsWith(QStringLiteral("P")), qPrintable(godet.sku));
    QVERIFY2(godet.sku.endsWith(QStringLiteral("GODET")), qPrintable(godet.sku));
}

void TestCatalog::deactivateHidesFromSearch()
{
    SqliteProductRepository products(m_db->connectionName());
    QVERIFY(products.setActive(m_productId, false).isOk());

    // Masqué par défaut (RG-02.a : désactivation, pas suppression)…
    QCOMPARE(products.search(QStringLiteral("roma")).value().size(), 0);
    // …mais toujours visible avec includeInactive.
    QCOMPARE(products.search(QStringLiteral("roma"), 0, true).value().size(), 1);

    QVERIFY(products.setActive(m_productId, true).isOk());
    QCOMPARE(products.search(QStringLiteral("roma")).value().size(), 1);
}

void TestCatalog::updateProductFields()
{
    SqliteProductRepository products(m_db->connectionName());
    Product product = products.byId(m_productId).value();
    product.nameFr = QStringLiteral("Romarin officinal");
    product.type = ProductType::Goods;
    product.descriptionFr = QStringLiteral("Aromatique méditerranéenne");
    QVERIFY(products.update(product).isOk());

    const Product reloaded = products.byId(m_productId).value();
    QCOMPARE(reloaded.nameFr, QStringLiteral("Romarin officinal"));
    QCOMPARE(reloaded.type, ProductType::Goods);
    QCOMPARE(reloaded.descriptionFr, QStringLiteral("Aromatique méditerranéenne"));
}

void TestCatalog::addAndUpdateVariants()
{
    SqliteProductRepository products(m_db->connectionName());

    // Ajout d'un 3e conditionnement avec code-barres et prix pro
    Variant pot17;
    pot17.productId = m_productId;
    pot17.packaging = QStringLiteral("pot17");
    pot17.barcode = QStringLiteral("6191234567890");
    pot17.priceTtc = Money::fromMillimes(12000);
    pot17.priceProTtc = Money::fromMillimes(9000);
    pot17.vatRatePercent = 19;
    pot17.alertThreshold = 5;
    QVERIFY(products.insertVariant(pot17).isOk());

    auto variants = products.variantsOf(m_productId).value();
    QCOMPARE(variants.size(), 3);
    Variant added;
    for (const Variant& v : variants)
        if (v.packaging == QLatin1String("pot17"))
            added = v;
    QVERIFY(added.id > 0);
    QCOMPARE(added.barcode, QStringLiteral("6191234567890"));
    QCOMPARE(added.priceProTtc, Money::fromMillimes(9000));
    QCOMPARE(added.alertThreshold, 5);

    // Modification du prix
    added.priceTtc = Money::fromMillimes(13500);
    QVERIFY(products.updateVariant(added).isOk());
    for (const Variant& v : products.variantsOf(m_productId).value())
        if (v.id == added.id)
            QCOMPARE(v.priceTtc, Money::fromMillimes(13500));
}

void TestCatalog::searchByBarcode()
{
    // Le scan d'un code-barres retrouve le produit (F02-06, jardinerie)
    SqliteProductRepository products(m_db->connectionName());
    const auto found = products.search(QStringLiteral("6191234567890"));
    QVERIFY(found.isOk());
    QCOMPARE(found.value().size(), 1);
    QCOMPARE(found.value().first().id, m_productId);
}

void TestCatalog::barcodeDuplicateDetected()
{
    // Un doublon de code-barres rendrait le scan de caisse ambigu.
    SqliteProductRepository products(m_db->connectionName());
    int holder = 0;
    for (const Variant& v : products.variantsOf(m_productId).value())
        if (v.packaging == QLatin1String("pot17"))
            holder = v.id;
    QVERIFY(holder > 0);

    QVERIFY(products.barcodeExists(QStringLiteral("6191234567890")).value());
    // La variante qui le porte déjà est exclue du contrôle (édition).
    QVERIFY(!products.barcodeExists(QStringLiteral("6191234567890"), holder)
                 .value());
    QVERIFY(!products.barcodeExists(QStringLiteral("0000000000000")).value());
    QVERIFY(!products.barcodeExists(QString()).value());
}

void TestCatalog::mainPhotoRoundTrip()
{
    SqliteProductRepository products(m_db->connectionName());

    // Aucune photo au départ
    QVERIFY(products.mainPhotoPath(m_productId).value().isEmpty());

    QVERIFY(products.setMainPhoto(m_productId,
                                  QStringLiteral("/data/photos/a.jpg")).isOk());
    QCOMPARE(products.mainPhotoPath(m_productId).value(),
             QStringLiteral("/data/photos/a.jpg"));

    // La recherche remonte le chemin de la photo principale (vignette)
    QCOMPARE(products.search(QStringLiteral("roma")).value().first().photoPath,
             QStringLiteral("/data/photos/a.jpg"));

    // Un seul cliché principal : le nouveau remplace l'ancien
    QVERIFY(products.setMainPhoto(m_productId,
                                  QStringLiteral("/data/photos/b.jpg")).isOk());
    QCOMPARE(products.mainPhotoPath(m_productId).value(),
             QStringLiteral("/data/photos/b.jpg"));
    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM product_photos WHERE is_main = 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
