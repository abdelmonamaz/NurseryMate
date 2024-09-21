#include "controllers/supplier_controller.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_supplier_repository.h"

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestSuppliers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createAndSearch();
    void receiptFeedsStockAndCost();
    void weightedAverageCost();
    void invalidReceiptsRejected();
    void recentReceiptsListed();
    void supplierProductsAndBestOffer();
    void paymentsBalanceAndDueCheques();
    void orderPreparationMatching();

private:
    Money avgCostOf(int variantId) const;

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSupplierRepository* m_suppliers = nullptr;
    SqliteProductRepository* m_products = nullptr;
    int m_supplierId = 0;
    int m_variantId = 0;
    int m_serre = 0;
};

Money TestSuppliers::avgCostOf(int variantId) const
{
    QSqlQuery query(m_db->database());
    query.exec(QStringLiteral("SELECT avg_cost FROM variants WHERE id = %1")
                   .arg(variantId));
    query.next();
    return Money::fromMillimes(query.value(0).toLongLong());
}

void TestSuppliers::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("sup.db")),
                               QStringLiteral("tst_suppliers"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_suppliers = new SqliteSupplierRepository(m_db->connectionName(), *m_stock);
    m_products = new SqliteProductRepository(m_db->connectionName());

    SqliteLocationRepository locations(m_db->connectionName());
    m_serre = locations.all().value().first().id;

    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    const int productId = m_products->insertWithVariants(romarin, {godet}).value();
    m_variantId = m_products->variantsOf(productId).value().first().id;
}

void TestSuppliers::createAndSearch()
{
    Supplier capBon;
    capBon.name = QStringLiteral("Pépinières du Cap Bon");
    capBon.phone = QStringLiteral("72555444");
    const auto created = m_suppliers->insert(capBon);
    QVERIFY(created.isOk());
    m_supplierId = created.value();

    QCOMPARE(m_suppliers->search(QStringLiteral("cap")).value().size(), 1);
    QCOMPARE(m_suppliers->search(QStringLiteral("72555")).value().size(), 1);
    QCOMPARE(m_suppliers->search(QStringLiteral("inconnu")).value().size(), 0);

    // Fiche enrichie (migration 014) : adresse exacte, produits fournis,
    // coordonnées GPS pour la future carte — aller-retour complet.
    Supplier full = m_suppliers->search(QStringLiteral("cap")).value().first();
    full.address = QStringLiteral("Route de Soliman km 3, Menzel Bouzelfa");
    full.supplies = QStringLiteral("plants d'agrumes, porte-greffes");
    full.paymentTerms = QStringLiteral("30 j fin de mois");
    full.latitude = 36.6811;
    full.longitude = 10.5847;
    QVERIFY(m_suppliers->update(full).isOk());

    const Supplier reread =
        m_suppliers->search(QStringLiteral("cap")).value().first();
    QCOMPARE(reread.address, full.address);
    QCOMPARE(reread.supplies, full.supplies);
    QCOMPARE(reread.paymentTerms, full.paymentTerms);
    QCOMPARE(reread.latitude, 36.6811);
    QCOMPARE(reread.longitude, 10.5847);
}

void TestSuppliers::receiptFeedsStockAndCost()
{
    // Réception : 100 godets à 1,200 (F07-04)
    ReceiptLine line;
    line.variantId = m_variantId;
    line.qty = 100;
    line.unitCost = Money::fromMillimes(1200);

    ReceiptDraft draft;
    draft.supplierId = m_supplierId;
    draft.locationId = m_serre;
    draft.lines = {line};
    const auto receipt = m_suppliers->recordReceipt(draft);
    QVERIFY2(receipt.isOk(),
             qPrintable(receipt.isOk() ? QString() : receipt.error().message));

    // Stock alimenté par un mouvement ref_kind='purchase'
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 100);
    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM stock_moves WHERE ref_kind = 'purchase'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    // Premier achat : CMP = coût d'achat (F03-09)
    QCOMPARE(avgCostOf(m_variantId), Money::fromMillimes(1200));
}

void TestSuppliers::weightedAverageCost()
{
    // 2e réception : 50 à 1,800 -> CMP = (100×1200 + 50×1800) / 150 = 1400
    ReceiptLine line;
    line.variantId = m_variantId;
    line.qty = 50;
    line.unitCost = Money::fromMillimes(1800);

    ReceiptDraft draft;
    draft.locationId = m_serre; // sans fournisseur (achat au marché)
    draft.lines = {line};
    QVERIFY(m_suppliers->recordReceipt(draft).isOk());

    QCOMPARE(avgCostOf(m_variantId), Money::fromMillimes(1400));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 150);
}

void TestSuppliers::invalidReceiptsRejected()
{
    ReceiptDraft empty;
    empty.locationId = m_serre;
    QVERIFY(!m_suppliers->recordReceipt(empty).isOk());

    ReceiptLine badQty;
    badQty.variantId = m_variantId;
    badQty.qty = 0;
    badQty.unitCost = Money::fromMillimes(100);
    ReceiptDraft draft;
    draft.locationId = m_serre;
    draft.lines = {badQty};
    QVERIFY(!m_suppliers->recordReceipt(draft).isOk());

    draft.lines.first().qty = 10;
    draft.locationId = 0;
    QVERIFY(!m_suppliers->recordReceipt(draft).isOk());

    // Rien n'a bougé
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 150);
    QCOMPARE(avgCostOf(m_variantId), Money::fromMillimes(1400));
}

void TestSuppliers::recentReceiptsListed()
{
    const auto receipts = m_suppliers->recentReceipts();
    QVERIFY(receipts.isOk());
    QCOMPARE(receipts.value().size(), 2);

    // Plus récente d'abord : l'achat au marché (50 × 1,800 = 90,000)
    QCOMPARE(receipts.value().first().supplierName, QString());
    QCOMPARE(receipts.value().first().totalQty, 50);
    QCOMPARE(receipts.value().first().totalCost, Money::fromMillimes(90000));
    // La première : Cap Bon (100 × 1,200 = 120,000)
    QCOMPARE(receipts.value().last().supplierName,
             QStringLiteral("Pépinières du Cap Bon"));
    QCOMPARE(receipts.value().last().totalCost, Money::fromMillimes(120000));
    QCOMPARE(receipts.value().last().locationFr, QStringLiteral("Serre 1"));
}

void TestSuppliers::supplierProductsAndBestOffer()
{
    // État hérité : Cap Bon a livré 100 romarins à 1,200 ; l'achat au
    // marché (sans fournisseur) 50 à 1,800 n'entre pas dans la comparaison.

    // Lien déclaratif + doublon inoffensif (F07-06).
    QVERIFY(m_suppliers->linkProduct(m_supplierId, m_variantId).isOk());
    QVERIFY(m_suppliers->linkProduct(m_supplierId, m_variantId).isOk());

    // Produits du fournisseur : lié + stats calculées des réceptions.
    auto products = m_suppliers->productsOf(m_supplierId);
    QVERIFY(products.isOk());
    QCOMPARE(products.value().size(), 1);
    QCOMPARE(products.value().first().variantId, m_variantId);
    QVERIFY(products.value().first().linked);
    QCOMPARE(products.value().first().qtySupplied, 100);
    QCOMPARE(products.value().first().deliveryCount, 1);
    QCOMPARE(products.value().first().lastCost, Money::fromMillimes(1200));
    QCOMPARE(products.value().first().avgCost, Money::fromMillimes(1200));

    // 2e fournisseur moins cher : il livre 30 à 1,000 -> meilleure offre.
    Supplier sahel;
    sahel.name = QStringLiteral("Graines du Sahel");
    const int sahelId = m_suppliers->insert(sahel).value();
    ReceiptLine line;
    line.variantId = m_variantId;
    line.qty = 30;
    line.unitCost = Money::fromMillimes(1000);
    ReceiptDraft draft;
    draft.supplierId = sahelId;
    draft.locationId = m_serre;
    draft.lines = {line};
    QVERIFY(m_suppliers->recordReceipt(draft).isOk());

    // Livré mais non lié : apparaît quand même, marqué non lié.
    const auto sahelProducts = m_suppliers->productsOf(sahelId);
    QCOMPARE(sahelProducts.value().size(), 1);
    QVERIFY(!sahelProducts.value().first().linked);

    // Meilleure offre (F07-09) : Sahel (1,000) devant Cap Bon (1,200).
    const auto offers = m_suppliers->suppliersFor(m_variantId);
    QVERIFY(offers.isOk());
    QCOMPARE(offers.value().size(), 2);
    QCOMPARE(offers.value().at(0).supplierName,
             QStringLiteral("Graines du Sahel"));
    QCOMPARE(offers.value().at(0).lastCost, Money::fromMillimes(1000));
    QCOMPARE(offers.value().at(1).supplierName,
             QStringLiteral("Pépinières du Cap Bon"));
    QCOMPARE(offers.value().at(1).lastCost, Money::fromMillimes(1200));

    // Évolution du prix d'achat (F07-10) : 1,200 -> 1,800 -> 1,000,
    // ordre chronologique (l'achat au marché EST dans l'historique).
    const auto history = m_suppliers->priceHistoryOf(m_variantId);
    QVERIFY(history.isOk());
    QCOMPARE(history.value().size(), 3);
    QCOMPARE(history.value().at(0).unitCost, Money::fromMillimes(1200));
    QCOMPARE(history.value().at(1).unitCost, Money::fromMillimes(1800));
    QCOMPARE(history.value().at(2).unitCost, Money::fromMillimes(1000));
    QCOMPARE(history.value().at(2).supplierName,
             QStringLiteral("Graines du Sahel"));

    // Retrait du lien : Cap Bon garde ses stats (livraisons réelles).
    QVERIFY(m_suppliers->unlinkProduct(m_supplierId, m_variantId).isOk());
    products = m_suppliers->productsOf(m_supplierId);
    QCOMPARE(products.value().size(), 1);
    QVERIFY(!products.value().first().linked);
    QCOMPARE(products.value().first().qtySupplied, 100);
}

void TestSuppliers::paymentsBalanceAndDueCheques()
{
    // Dette héritée de Cap Bon : réception 100 × 1,200 = 120,000.
    QCOMPARE(m_suppliers->supplierBalance(m_supplierId).value(),
             Money::fromMillimes(120000));
    // Visible dans la recherche (champ balance calculé).
    QCOMPARE(m_suppliers->search(QStringLiteral("cap")).value().first().balance,
             Money::fromMillimes(120000));

    // Acompte espèces 50,000 -> dette 70,000.
    QVERIFY(m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(50000), QStringLiteral("cash"),
        {}, {}, QStringLiteral("acompte"), 0).isOk());
    QCOMPARE(m_suppliers->supplierBalance(m_supplierId).value(),
             Money::fromMillimes(70000));

    // Chèque sans échéance refusé ; avec échéance passée -> ÉCHU.
    QVERIFY(!m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(10000), QStringLiteral("cheque"),
        QStringLiteral("123456"), {}, {}, 0).isOk());
    QVERIFY(m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(30000), QStringLiteral("cheque"),
        QStringLiteral("123456"), QStringLiteral("2020-01-01"), {}, 0).isOk());
    // Chèque à échéance future proche (dans 10 jours).
    const QString soon =
        QDate::currentDate().addDays(10).toString(Qt::ISODate);
    QVERIFY(m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(20000), QStringLiteral("cheque"),
        QStringLiteral("123457"), soon, {}, 0).isOk());
    // Chèque à 90 jours : hors fenêtre des 30 jours.
    QVERIFY(m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(5000), QStringLiteral("cheque"),
        QStringLiteral("123458"),
        QDate::currentDate().addDays(90).toString(Qt::ISODate), {}, 0).isOk());

    // Dette finale : 120,000 − 50,000 − 30,000 − 20,000 − 5,000 = 15,000.
    QCOMPARE(m_suppliers->supplierBalance(m_supplierId).value(),
             Money::fromMillimes(15000));

    // Échéances ≤ 30 j : l'échu d'abord, puis le proche ; pas le lointain.
    const auto due = m_suppliers->dueCheques();
    QVERIFY(due.isOk());
    QCOMPARE(due.value().size(), 2);
    QVERIFY(due.value().at(0).overdue);
    QCOMPARE(due.value().at(0).amount, Money::fromMillimes(30000));
    QVERIFY(!due.value().at(1).overdue);
    QCOMPARE(due.value().at(1).dueDate, soon);

    // Journal : plus récents d'abord (4 paiements).
    const auto payments = m_suppliers->paymentsOf(m_supplierId);
    QCOMPARE(payments.value().size(), 4);
    QCOMPARE(payments.value().last().note, QStringLiteral("acompte"));

    // Montant nul refusé.
    QVERIFY(!m_suppliers->recordSupplierPayment(
        m_supplierId, Money::fromMillimes(0), QStringLiteral("cash"),
        {}, {}, {}, 0).isOk());
}

void TestSuppliers::orderPreparationMatching()
{
    // Préparation de commande : 2 lignes, 2 fournisseurs candidats.
    Product olivier;
    olivier.nameFr = QStringLiteral("Olivier");
    Variant pot21;
    pot21.packaging = QStringLiteral("pot21");
    pot21.priceTtc = Money::fromMillimes(35000);
    const int olivierId =
        m_products->insertWithVariants(olivier, {pot21}).value();
    const int olivierVar =
        m_products->variantsOf(olivierId).value().first().id;

    Supplier sahel;
    sahel.name = QStringLiteral("Sahel Plants");
    const int sahelId = m_suppliers->insert(sahel).value();

    // Sahel livre LES DEUX produits : romarin à 2,000, olivier à 9,000.
    ReceiptLine romarinLine;
    romarinLine.variantId = m_variantId;
    romarinLine.qty = 40;
    romarinLine.unitCost = Money::fromMillimes(2000);
    ReceiptLine olivierLine;
    olivierLine.variantId = olivierVar;
    olivierLine.qty = 5;
    olivierLine.unitCost = Money::fromMillimes(9000);
    ReceiptDraft sahelDraft;
    sahelDraft.supplierId = sahelId;
    sahelDraft.locationId = m_serre;
    sahelDraft.lines = {romarinLine, olivierLine};
    QVERIFY(m_suppliers->recordReceipt(sahelDraft).isOk());

    SupplierController controller(*m_suppliers);
    const QVariantList lines{
        QVariantMap{{QStringLiteral("variantId"), m_variantId},
                    {QStringLiteral("qty"), 10},
                    {QStringLiteral("label"), QStringLiteral("Romarin — godet")}},
        QVariantMap{{QStringLiteral("variantId"), olivierVar},
                    {QStringLiteral("qty"), 2},
                    {QStringLiteral("label"), QStringLiteral("Olivier — pot21")}},
    };

    // Tri prix : Sahel couvre 2/2 -> en tête, total = 10×2,000 + 2×9,000.
    const QVariantList byPrice =
        controller.supplierMatches(lines, QStringLiteral("price"));
    QVERIFY(byPrice.size() >= 2);
    const QVariantMap best = byPrice.first().toMap();
    QCOMPARE(best.value(QStringLiteral("name")).toString(),
             QStringLiteral("Sahel Plants"));
    QVERIFY(best.value(QStringLiteral("full")).toBool());
    QCOMPARE(best.value(QStringLiteral("coverageLabel")).toString(),
             QStringLiteral("2/2"));
    QVERIFY(best.value(QStringLiteral("estimatedDisplay")).toString()
                .contains(Money::fromMillimes(38000)
                              .toDisplayString(QLocale())));

    // Cap Bon ne couvre que le romarin : couverture partielle + manque.
    QVariantMap capBon;
    for (const QVariant& row : byPrice)
        if (row.toMap().value(QStringLiteral("supplierId")).toInt()
            == m_supplierId)
            capBon = row.toMap();
    QVERIFY(!capBon.isEmpty());
    QVERIFY(!capBon.value(QStringLiteral("full")).toBool());
    QCOMPARE(capBon.value(QStringLiteral("missingLabels")).toString(),
             QStringLiteral("Olivier — pot21"));

    // Tri quantité : la couverture prime toujours — Sahel reste en tête.
    const QVariantList byQty =
        controller.supplierMatches(lines, QStringLiteral("quantity"));
    QCOMPARE(byQty.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Sahel Plants"));
}

QTEST_GUILESS_MAIN(TestSuppliers)
#include "tst_suppliers.moc"
