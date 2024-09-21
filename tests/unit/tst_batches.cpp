#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_batch_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestBatches : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createNumberedBatch();
    void lossReducesRemaining();
    void sellableFeedsStock();
    void overConsumeRejected();
    void invariantHolds();
    void cancelLastEventRestores();
    void deleteOnlyIfNeverUsed();
    void treatmentsJournal();

private:
    BatchRow currentBatch();

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteBatchRepository* m_batches = nullptr;
    int m_productId = 0;
    int m_variantId = 0;
    int m_serre = 0;
    int m_vente = 0;
    int m_batchId = 0;
};

BatchRow TestBatches::currentBatch()
{
    for (const BatchRow& row : m_batches->list(true).value())
        if (row.id == m_batchId)
            return row;
    return {};
}

void TestBatches::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("batch.db")),
                               QStringLiteral("tst_batches"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_batches = new SqliteBatchRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    m_serre = locations.all().value().at(0).id; // Serre 1
    m_vente = locations.all().value().at(1).id; // Zone de vente

    SqliteProductRepository products(m_db->connectionName());
    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    m_productId = products.insertWithVariants(romarin, {godet}).value();
    m_variantId = products.variantsOf(m_productId).value().first().id;
}

void TestBatches::createNumberedBatch()
{
    BatchDraft draft;
    draft.productId = m_productId;
    draft.origin = BatchOrigin::Cutting;
    draft.qtyInitial = 500;
    draft.locationId = m_serre;
    const auto batch = m_batches->create(draft);
    QVERIFY2(batch.isOk(),
             qPrintable(batch.isOk() ? QString() : batch.error().message));
    QVERIFY(batch.value().number.startsWith(QStringLiteral("L-")));
    QVERIFY(batch.value().number.endsWith(QStringLiteral("001")));
    m_batchId = batch.value().id;

    const BatchRow row = currentBatch();
    QCOMPARE(row.qtyInitial, 500);
    QCOMPARE(row.qtyRemaining, 500);
    QCOMPARE(row.status, BatchStatus::Growing);

    // Refus si quantité initiale nulle
    BatchDraft bad;
    bad.productId = m_productId;
    bad.qtyInitial = 0;
    QVERIFY(!m_batches->create(bad).isOk());
}

void TestBatches::lossReducesRemaining()
{
    // 80 pertes par gel (F08-02)
    QVERIFY(m_batches->recordLoss(m_batchId, 80, QStringLiteral("frost"),
                                  QStringLiteral("vague de froid"), 0)
                .isOk());
    const BatchRow row = currentBatch();
    QCOMPARE(row.qtyRemaining, 420);
    QCOMPARE(row.qtyLost, 80);
    // Survie = (500 - 80) / 500 = 84 %
    QCOMPARE(row.survivalPercent(), 84);
}

void TestBatches::sellableFeedsStock()
{
    // 300 passent en vendable -> entrée de stock commercial (RG-08.a)
    QVERIFY(m_batches->recordSellable(m_batchId, 300, m_variantId, m_vente, 0)
                .isOk());

    const BatchRow row = currentBatch();
    QCOMPARE(row.qtyRemaining, 120); // 420 - 300
    QCOMPARE(row.qtySellable, 300);

    // Le stock commercial a été alimenté par un mouvement ref_kind='batch'
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 300);
    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM stock_moves WHERE ref_kind = 'batch'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestBatches::overConsumeRejected()
{
    // Il reste 120 : impossible d'en perdre 200 ni d'en vendre 200
    QVERIFY(!m_batches->recordLoss(m_batchId, 200, QStringLiteral("other"),
                                   QString(), 0)
                 .isOk());
    QVERIFY(!m_batches->recordSellable(m_batchId, 200, m_variantId, m_vente, 0)
                 .isOk());
    QCOMPARE(currentBatch().qtyRemaining, 120); // inchangé
}

void TestBatches::invariantHolds()
{
    // RG-08.b : initial = restant + pertes + vendables
    const BatchRow row = currentBatch();
    QCOMPARE(row.qtyRemaining + row.qtyLost + row.qtySellable, row.qtyInitial);

    // Vider le reste -> lot clôturé automatiquement
    QVERIFY(m_batches->recordSellable(m_batchId, 120, m_variantId, m_vente, 0)
                .isOk());
    QCOMPARE(currentBatch().status, BatchStatus::Closed);
    QCOMPARE(currentBatch().qtyRemaining, 0);
    // N'apparaît plus dans la liste des lots en cours
    QCOMPARE(m_batches->list(false).value().size(), 0);
}

void TestBatches::cancelLastEventRestores()
{
    // Annulation du dernier événement (les 120 passés en vendable) :
    // contre-événement tracé, restant restauré, lot ROUVERT, stock ressorti.
    QVERIFY2(m_batches->cancelLastEvent(m_batchId, 0).isOk(), "cancel");

    const BatchRow row = currentBatch();
    QCOMPARE(row.qtyRemaining, 120);
    QCOMPARE(row.status, BatchStatus::Growing); // rouvert
    QCOMPARE(row.qtySellable, 300);             // net : 420 − 120
    QCOMPARE(row.qtyRemaining + row.qtyLost + row.qtySellable,
             row.qtyInitial); // invariant RG-08.b préservé

    // Stock commercial ressorti : 420 − 120 = 300
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 300);

    // Le journal trace la correction (qty négative en tête)
    const auto events = m_batches->events(m_batchId);
    QVERIFY(events.isOk());
    QCOMPARE(events.value().first().qty, -120);
    QCOMPARE(events.value().first().kind, QStringLiteral("sellable"));

    // On n'annule pas une annulation : ressaisir le bon événement.
    const auto again = m_batches->cancelLastEvent(m_batchId, 0);
    QVERIFY(!again.isOk());
    QCOMPARE(again.error().code, QStringLiteral("batch.alreadyCancelled"));
}

void TestBatches::deleteOnlyIfNeverUsed()
{
    // Le lot historique a des événements : suppression refusée (norme).
    QVERIFY(m_batches->isReferenced(m_batchId).value());
    const auto refused = m_batches->remove(m_batchId);
    QVERIFY(!refused.isOk());
    QCOMPARE(refused.error().code, QStringLiteral("batch.referenced"));

    // Un lot fraîchement créé, jamais utilisé, se supprime.
    BatchDraft draft;
    draft.productId = m_productId;
    draft.qtyInitial = 50;
    draft.locationId = m_serre;
    const auto fresh = m_batches->create(draft);
    QVERIFY(fresh.isOk());
    QVERIFY(!m_batches->isReferenced(fresh.value().id).value());
    QVERIFY(m_batches->remove(fresh.value().id).isOk());
    for (const BatchRow& row : m_batches->list(true).value())
        QVERIFY(row.id != fresh.value().id);
}

void TestBatches::treatmentsJournal()
{
    // F08-03 : journal des interventions par lot.
    QVERIFY(m_batches->recordTreatment(
        m_batchId, QStringLiteral("phyto"),
        QStringLiteral("bouillie bordelaise"), QStringLiteral("20 g/L"),
        QStringLiteral("préventif mildiou"), 0).isOk());
    QVERIFY(m_batches->recordTreatment(
        m_batchId, QStringLiteral("watering"), QString(), QString(),
        QString(), 0).isOk());

    // Phyto sans produit : refusé (traçabilité) ; type inconnu : refusé.
    QVERIFY(!m_batches->recordTreatment(m_batchId, QStringLiteral("phyto"),
                                        QString(), QString(), QString(), 0)
                 .isOk());
    QVERIFY(!m_batches->recordTreatment(m_batchId, QStringLiteral("magie"),
                                        QString(), QString(), QString(), 0)
                 .isOk());

    const auto treatments = m_batches->treatments(m_batchId);
    QVERIFY(treatments.isOk());
    QCOMPARE(treatments.value().size(), 2);
    QCOMPARE(treatments.value().first().kind, QStringLiteral("watering"));
    QCOMPARE(treatments.value().last().productUsed,
             QStringLiteral("bouillie bordelaise"));
    QCOMPARE(treatments.value().last().dose, QStringLiteral("20 g/L"));
}

QTEST_GUILESS_MAIN(TestBatches)
#include "tst_batches.moc"
