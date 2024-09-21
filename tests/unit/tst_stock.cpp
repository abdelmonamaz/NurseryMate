#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestStock : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void locationsSeeded();
    void entryAddsStock();
    void transferSplitsStock();
    void exitCanGoNegative();
    void adjustBothDirections();
    void invalidMovesRejected();
    void idempotentByUuid();
    void overviewAndThreshold();
    void searchVariantsWorks();
    void historyListsMoves();
    void historyExposesCounterMoveData();
    void typedLossOnStockOut();

private:
    StockMove makeMove(MoveKind kind, int from, int to, int qty) const;

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    int m_variantId = 0;
    int m_serre = 0;
    int m_vente = 0;
};

StockMove TestStock::makeMove(MoveKind kind, int from, int to, int qty) const
{
    StockMove move;
    move.kind = kind;
    move.variantId = m_variantId;
    move.fromLocationId = from;
    move.toLocationId = to;
    move.qty = qty;
    return move;
}

void TestStock::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("stock.db")),
                               QStringLiteral("tst_stock"));
    const auto opened = m_db->open();
    QVERIFY2(opened.isOk(),
             qPrintable(opened.isOk() ? QString() : opened.error().message));
    QCOMPARE(m_db->schemaVersion(), 20);

    // Produit de travail : Romarin godet
    SqliteProductRepository products(m_db->connectionName());
    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    romarin.nameAr = QStringLiteral("إكليل الجبل");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    const auto created = products.insertWithVariants(romarin, {godet});
    QVERIFY(created.isOk());
    const auto variants = products.variantsOf(created.value());
    QVERIFY(variants.isOk());
    m_variantId = variants.value().first().id;
}

void TestStock::locationsSeeded()
{
    SqliteLocationRepository locations(m_db->connectionName());
    const auto all = locations.all();
    QVERIFY(all.isOk());
    QCOMPARE(all.value().size(), 3);
    m_serre = all.value().at(0).id;   // Serre 1
    m_vente = all.value().at(1).id;   // Zone de vente
    QCOMPARE(all.value().at(0).kind, LocationKind::Greenhouse);
    QCOMPARE(all.value().at(1).kind, LocationKind::SalesArea);
    QCOMPARE(all.value().at(1).nameAr, QStringLiteral("منطقة البيع"));
}

void TestStock::entryAddsStock()
{
    SqliteStockRepository stock(m_db->connectionName());
    QVERIFY(stock.recordMove(makeMove(MoveKind::In, 0, m_serre, 100)).isOk());

    const auto levels = stock.levelsOf(m_variantId);
    QVERIFY(levels.isOk());
    QCOMPARE(levels.value().size(), 1);
    QCOMPARE(levels.value().first().qty, 100);
}

void TestStock::transferSplitsStock()
{
    SqliteStockRepository stock(m_db->connectionName());
    QVERIFY(stock.recordMove(makeMove(MoveKind::Transfer, m_serre, m_vente, 30)).isOk());

    const auto levels = stock.levelsOf(m_variantId);
    QVERIFY(levels.isOk());
    QCOMPARE(levels.value().size(), 2);
    QCOMPARE(levels.value().at(0).qty, 70);  // Serre 1
    QCOMPARE(levels.value().at(1).qty, 30);  // Zone de vente
}

void TestStock::exitCanGoNegative()
{
    // RG-03.b : vendre plus que le stock connu est autorisé, jamais bloqué.
    SqliteStockRepository stock(m_db->connectionName());
    QVERIFY(stock.recordMove(makeMove(MoveKind::Out, m_vente, 0, 50)).isOk());

    const auto levels = stock.levelsOf(m_variantId);
    QVERIFY(levels.isOk());
    QCOMPARE(levels.value().at(1).qty, -20); // Zone de vente
}

void TestStock::adjustBothDirections()
{
    SqliteStockRepository stock(m_db->connectionName());
    // Écart positif : destination seule
    QVERIFY(stock.recordMove(makeMove(MoveKind::Adjust, 0, m_serre, 5)).isOk());
    QCOMPARE(stock.levelsOf(m_variantId).value().at(0).qty, 75);
    // Écart négatif : origine seule
    QVERIFY(stock.recordMove(makeMove(MoveKind::Adjust, m_serre, 0, 5)).isOk());
    QCOMPARE(stock.levelsOf(m_variantId).value().at(0).qty, 70);
}

void TestStock::invalidMovesRejected()
{
    SqliteStockRepository stock(m_db->connectionName());
    // Quantité nulle
    QVERIFY(!stock.recordMove(makeMove(MoveKind::In, 0, m_serre, 0)).isOk());
    // Entrée sans destination
    QVERIFY(!stock.recordMove(makeMove(MoveKind::In, 0, 0, 10)).isOk());
    // Sortie sans origine
    QVERIFY(!stock.recordMove(makeMove(MoveKind::Out, 0, 0, 10)).isOk());
    // Transfert vers le même emplacement
    QVERIFY(!stock.recordMove(makeMove(MoveKind::Transfer, m_serre, m_serre, 10)).isOk());
    // Ajustement avec les deux emplacements
    QVERIFY(!stock.recordMove(makeMove(MoveKind::Adjust, m_serre, m_vente, 10)).isOk());
    // Variante manquante
    StockMove noVariant = makeMove(MoveKind::In, 0, m_serre, 10);
    noVariant.variantId = 0;
    QVERIFY(!stock.recordMove(noVariant).isOk());

    // Rien n'a bougé
    QCOMPARE(stock.levelsOf(m_variantId).value().at(0).qty, 70);
}

void TestStock::idempotentByUuid()
{
    // Rejeu de sync (doc 02 §5.1) : le même uuid ne s'applique qu'une fois.
    SqliteStockRepository stock(m_db->connectionName());
    StockMove move = makeMove(MoveKind::In, 0, m_serre, 10);
    move.uuid = QStringLiteral("11111111-2222-3333-4444-555555555555");

    QVERIFY(stock.recordMove(move).isOk());
    QCOMPARE(stock.levelsOf(m_variantId).value().at(0).qty, 80);

    QVERIFY(stock.recordMove(move).isOk()); // rejeu -> succès silencieux
    QCOMPARE(stock.levelsOf(m_variantId).value().at(0).qty, 80); // inchangé
}

void TestStock::overviewAndThreshold()
{
    SqliteProductRepository products(m_db->connectionName());
    SqliteStockRepository stock(m_db->connectionName());

    // Total = Serre 80 + Vente -20 = 60
    auto rows = stock.overview(QString());
    QVERIFY(rows.isOk());
    QCOMPARE(rows.value().size(), 1);
    QCOMPARE(rows.value().first().qty, 60);
    QCOMPARE(rows.value().first().alertThreshold, -1);

    // Filtre par emplacement
    QCOMPARE(stock.overview(QString(), m_vente).value().first().qty, -20);

    // Seuil d'alerte (F03-05) remonté dans la vue
    const int productId =
        products.search(QStringLiteral("Romarin")).value().first().id;
    auto variants = products.variantsOf(productId);
    QVERIFY(variants.isOk());
    Variant godet = variants.value().first();
    godet.alertThreshold = 100;
    QVERIFY(products.updateVariant(godet).isOk());
    QCOMPARE(stock.overview(QString()).value().first().alertThreshold, 100);
}

void TestStock::searchVariantsWorks()
{
    SqliteStockRepository stock(m_db->connectionName());
    const auto picks = stock.searchVariants(QStringLiteral("roma"));
    QVERIFY(picks.isOk());
    QCOMPARE(picks.value().size(), 1);
    QCOMPARE(picks.value().first().label, QStringLiteral("Romarin — godet"));
    QCOMPARE(stock.searchVariants(QStringLiteral("tomate")).value().size(), 0);
}

void TestStock::historyListsMoves()
{
    // F03-07 : tous les mouvements valides du scénario, plus récents d'abord
    SqliteStockRepository stock(m_db->connectionName());
    const auto history = stock.history();
    QVERIFY(history.isOk());
    // in 100, transfer 30, out 50, adjust +5, adjust -5, in 10 (uuid fixe)
    QCOMPARE(history.value().size(), 6);
    QCOMPARE(history.value().first().kind, MoveKind::In); // le plus récent
    QCOMPARE(history.value().first().qty, 10);
    QCOMPARE(history.value().last().qty, 100);            // le premier
    QCOMPARE(history.value().last().toFr, QStringLiteral("Serre 1"));
    QCOMPARE(history.value().first().productFr, QStringLiteral("Romarin"));

    // Filtré par variante = identique ici (une seule variante)
    QCOMPARE(stock.history(m_variantId).value().size(), 6);
    QCOMPARE(stock.history(99999).value().size(), 0);
}

void TestStock::historyExposesCounterMoveData()
{
    // Correction guidée (↩ dans l'historique) : l'historique doit porter
    // tout ce qu'il faut pour pré-remplir le contre-mouvement.
    SqliteStockRepository stock(m_db->connectionName());
    const auto levelsBefore = stock.levelsOf(m_variantId);
    QVERIFY(levelsBefore.isOk());
    int serreBefore = 0;
    for (const StockLevel& level : levelsBefore.value())
        if (level.locationId == m_serre)
            serreBefore = level.qty;

    // Entrée erronée saisie à la main, note d'origine
    StockMove wrong = makeMove(MoveKind::In, 0, m_serre, 7);
    wrong.note = QStringLiteral("saisie erronée");
    QVERIFY(stock.recordMove(wrong).isOk());

    const auto history = stock.history(m_variantId, 1);
    QVERIFY(history.isOk());
    const StockMoveRow& top = history.value().first();
    QCOMPARE(top.kind, MoveKind::In);
    QCOMPARE(top.qty, 7);
    QCOMPARE(top.variantId, m_variantId);
    QCOMPARE(top.fromLocationId, 0);
    QCOMPARE(top.toLocationId, m_serre);
    QVERIFY(top.refKind.isEmpty()); // mouvement manuel -> corrigeable
    // La note du mouvement est visible dans l'historique (repli reason/note)
    QCOMPARE(top.reason, QStringLiteral("saisie erronée"));

    // Le contre-mouvement que le bouton ↩ pré-remplit : sortie DEPUIS
    // l'emplacement livré, même quantité — le niveau revient à l'initial.
    StockMove counter = makeMove(MoveKind::Out, m_serre, 0, 7);
    counter.note = QStringLiteral("Correction du mouvement #%1").arg(top.id);
    QVERIFY(stock.recordMove(counter).isOk());

    const auto levelsAfter = stock.levelsOf(m_variantId);
    QVERIFY(levelsAfter.isOk());
    for (const StockLevel& level : levelsAfter.value())
        if (level.locationId == m_serre)
            QCOMPARE(level.qty, serreBefore);
}

void TestStock::typedLossOnStockOut()
{
    // F03-08 : une sortie peut porter un motif de perte typé, restitué
    // par l'historique (rapports et lisibilité).
    SqliteStockRepository stock(m_db->connectionName());
    StockMove out = makeMove(MoveKind::Out, m_serre, 0, 2);
    out.lossReason = QStringLiteral("frost");
    out.note = QStringLiteral("gelée nocturne");
    QVERIFY(stock.recordMove(out).isOk());

    const auto history = stock.history(m_variantId, 1);
    QVERIFY(history.isOk());
    QCOMPARE(history.value().first().lossReason, QStringLiteral("frost"));
}

QTEST_GUILESS_MAIN(TestStock)
#include "tst_stock.moc"
