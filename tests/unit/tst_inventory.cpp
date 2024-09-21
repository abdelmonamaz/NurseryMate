#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_inventory_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestInventory : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void startSnapshotsStock();
    void resumeReturnsSameDraft();
    void countingLines();
    void validateCreatesAdjustments();
    void revalidationIsIdempotent();
    void validatedInventoryIsLocked();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteInventoryRepository* m_inventories = nullptr;
    int m_godet = 0;
    int m_pot14 = 0;
    int m_serre = 0;
    int m_inventoryId = 0;
    QString m_inventoryUuid;
};

void TestInventory::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("inv.db")),
                               QStringLiteral("tst_inventory"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_inventories = new SqliteInventoryRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    m_serre = locations.all().value().first().id;

    // Romarin godet (120 en serre) + pot14 (40 en serre)
    SqliteProductRepository products(m_db->connectionName());
    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    Variant pot14;
    pot14.packaging = QStringLiteral("pot14");
    const int productId =
        products.insertWithVariants(romarin, {godet, pot14}).value();
    const auto variants = products.variantsOf(productId).value();
    m_godet = variants.at(0).id;
    m_pot14 = variants.at(1).id;

    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_serre;
    entry.variantId = m_godet;
    entry.qty = 120;
    QVERIFY(m_stock->recordMove(entry).isOk());
    entry.uuid.clear();
    entry.variantId = m_pot14;
    entry.qty = 40;
    QVERIFY(m_stock->recordMove(entry).isOk());
}

void TestInventory::startSnapshotsStock()
{
    const auto inventory = m_inventories->startOrResume(m_serre);
    QVERIFY2(inventory.isOk(),
             qPrintable(inventory.isOk() ? QString() : inventory.error().message));
    m_inventoryId = inventory.value().id;
    m_inventoryUuid = inventory.value().uuid;
    QCOMPARE(inventory.value().status, InventoryStatus::Draft);

    const auto lines = m_inventories->linesOf(m_inventoryId).value();
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0).qtyExpected, 120); // godet
    QCOMPARE(lines.at(1).qtyExpected, 40);  // pot14
    QVERIFY(!lines.at(0).isCounted());
}

void TestInventory::resumeReturnsSameDraft()
{
    // Un seul brouillon par emplacement : redémarrer = reprendre.
    const auto resumed = m_inventories->startOrResume(m_serre);
    QVERIFY(resumed.isOk());
    QCOMPARE(resumed.value().id, m_inventoryId);
}

void TestInventory::countingLines()
{
    // godet : compté 115 (écart -5) ; pot14 : identique (écart 0)
    QVERIFY(m_inventories->setCounted(m_inventoryId, m_godet, 115).isOk());
    QVERIFY(m_inventories->setCounted(m_inventoryId, m_pot14, 40).isOk());

    const auto lines = m_inventories->linesOf(m_inventoryId).value();
    QCOMPARE(lines.at(0).qtyCounted, 115);
    QCOMPARE(lines.at(0).gap(), -5);
    QCOMPARE(lines.at(1).gap(), 0);
}

void TestInventory::validateCreatesAdjustments()
{
    const auto validated = m_inventories->validate(m_inventoryId);
    QVERIFY2(validated.isOk(),
             qPrintable(validated.isOk() ? QString() : validated.error().message));
    QCOMPARE(validated.value(), 1); // un seul écart -> un ajustement

    // Le stock reflète le comptage (RG-03.c)
    QCOMPARE(m_stock->levelsOf(m_godet).value().first().qty, 115);
    QCOMPARE(m_stock->levelsOf(m_pot14).value().first().qty, 40);
}

void TestInventory::revalidationIsIdempotent()
{
    // Un inventaire validé ne se revalide pas…
    QVERIFY(!m_inventories->validate(m_inventoryId).isOk());

    // …et même en rejouant l'ajustement à la main (crash simulé entre
    // les ajustements et le verrouillage), l'uuid v5 déterministe
    // empêche toute double application.
    StockMove replay;
    replay.kind = MoveKind::Adjust;
    replay.variantId = m_godet;
    replay.fromLocationId = m_serre;
    replay.qty = 5;
    replay.uuid = QUuid::createUuidV5(
                      QUuid(QStringLiteral("{7f0c2e4a-5b1d-4e8a-9c3f-2a6d8e4b1c05}")),
                      QStringLiteral("%1:%2").arg(m_inventoryUuid).arg(m_godet))
                      .toString(QUuid::WithoutBraces);
    QVERIFY(m_stock->recordMove(replay).isOk()); // succès silencieux
    QCOMPARE(m_stock->levelsOf(m_godet).value().first().qty, 115); // inchangé
}

void TestInventory::validatedInventoryIsLocked()
{
    // Plus de comptage ni d'annulation après validation (RG-03.c)
    QVERIFY(!m_inventories->setCounted(m_inventoryId, m_godet, 99).isOk());
    QVERIFY(!m_inventories->cancel(m_inventoryId).isOk());

    // Un nouvel inventaire du même emplacement repart du stock corrigé
    const auto next = m_inventories->startOrResume(m_serre);
    QVERIFY(next.isOk());
    QVERIFY(next.value().id != m_inventoryId);
    QCOMPARE(m_inventories->linesOf(next.value().id).value().at(0).qtyExpected, 115);
}

QTEST_GUILESS_MAIN(TestInventory)
#include "tst_inventory.moc"
