#include "common/password_hasher.h"
#include "database/database_manager.h"
#include "mobile_store.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_user_repository.h"
#include "sync_client.h"
#include "sync_server.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

// Chaîne mobile complète, en HTTP réel : login -> réplication locale ->
// saisie terrain HORS LIGNE -> sync (push idempotent + re-pull).
class TestMobileSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void discoveryFindsServer();
    void loginRepliesAndReplicates();
    void offlineQueueThenSync();
    void inventoryAdjustFlow();
    void photoReplication();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteUserRepository* m_users = nullptr;
    SyncServer* m_server = nullptr;
    MobileStore* m_store = nullptr;
    SyncClient* m_client = nullptr;
    quint16 m_port = 0;
    int m_variantId = 0;
    int m_serre = 0;
    int m_vente = 0;
};

void TestMobileSync::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("master.db")),
                               QStringLiteral("tst_mobile_master"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_users = new SqliteUserRepository(m_db->connectionName());

    User khaled;
    khaled.username = QStringLiteral("khaled");
    khaled.displayName = QStringLiteral("Khaled");
    khaled.role = UserRole::Worker;
    QVERIFY(m_users->insert(khaled, PasswordHasher::hash(
                                        QStringLiteral("3456"))).isOk());

    SqliteLocationRepository locations(m_db->connectionName());
    m_serre = locations.all().value().at(0).id;
    m_vente = locations.all().value().at(1).id;

    SqliteProductRepository products(m_db->connectionName());
    Product citronnier;
    citronnier.nameFr = QStringLiteral("Citronnier 4 saisons");
    Variant pot17;
    pot17.packaging = QStringLiteral("pot17");
    pot17.priceTtc = Money::fromMillimes(25000);
    const int productId =
        products.insertWithVariants(citronnier, {pot17}).value();
    m_variantId = products.variantsOf(productId).value().first().id;

    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_serre;
    entry.variantId = m_variantId;
    entry.qty = 30;
    QVERIFY(m_stock->recordMove(entry).isOk());

    m_server = new SyncServer(m_db->connectionName(), *m_users, *m_stock);
    const auto started = m_server->start(0);
    QVERIFY(started.isOk());
    m_port = started.value();

    m_store = new MobileStore(m_dir.filePath(QStringLiteral("mobile.db")),
                              QStringLiteral("tst_mobile_store"));
    QVERIFY(m_store->open().isOk());
    m_client = new SyncClient(*m_store);
}

void TestMobileSync::discoveryFindsServer()
{
    // F09-09 (à la place du mDNS) : broadcast UDP -> IP + port du poste.
    QSignalSpy spy(m_client, &SyncClient::discoverFinished);
    m_client->discover(QString::number(m_port));
    QVERIFY(spy.wait(3000));
    const QList<QVariant> args = spy.takeFirst();
    QVERIFY(args.at(0).toBool());
    // L'hôte peut être 127.0.0.1 ou l'IP LAN de la machine (broadcast) —
    // les deux joignent bien ce poste.
    QVERIFY(!args.at(1).toString().isEmpty());
    QCOMPARE(args.at(2).toString(), QString::number(m_port));
}

void TestMobileSync::loginRepliesAndReplicates()
{
    QSignalSpy loginSpy(m_client, &SyncClient::loginFinished);
    QSignalSpy syncSpy(m_client, &SyncClient::syncFinished);

    // Mauvais PIN d'abord
    m_client->login(QStringLiteral("127.0.0.1"), QString::number(m_port),
                    QStringLiteral("khaled"), QStringLiteral("0000"));
    QVERIFY(loginSpy.wait(5000));
    QCOMPARE(loginSpy.takeFirst().at(0).toBool(), false);

    // Bon PIN : login + première sync automatique (réplication)
    m_client->login(QStringLiteral("127.0.0.1"), QString::number(m_port),
                    QStringLiteral("khaled"), QStringLiteral("3456"));
    QVERIFY(loginSpy.wait(5000));
    QCOMPARE(loginSpy.takeFirst().at(0).toBool(), true);
    QVERIFY(m_client->connected());
    QVERIFY(syncSpy.wait(5000));
    QCOMPARE(syncSpy.takeFirst().at(0).toBool(), true);

    // La réplique locale répond hors ligne
    const QVariantList found =
        m_client->searchVariants(QStringLiteral("citron"));
    QCOMPARE(found.size(), 1);
    QCOMPARE(found.first().toMap().value(QStringLiteral("totalQty")).toInt(),
             30);
    QVERIFY(m_client->locationOptions().size() >= 3);
    QVERIFY(!m_client->lastSync().isEmpty());
}

void TestMobileSync::offlineQueueThenSync()
{
    // Transfert terrain 5 : Serre 1 -> Zone de vente. La saisie est
    // LOCALE (file + effet optimiste) — le serveur n'a rien reçu tant
    // que queueMove déclenche la sync opportuniste… qui tourne ici aussi ;
    // on vérifie l'état final : file vidée, serveur à jour, réplique vraie.
    QSignalSpy syncSpy(m_client, &SyncClient::syncFinished);
    QVERIFY(m_client->queueMove(QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("transfer")},
        {QStringLiteral("variantId"), m_variantId},
        {QStringLiteral("fromId"), m_serre},
        {QStringLiteral("toId"), m_vente},
        {QStringLiteral("qty"), 5},
    }));
    QVERIFY(syncSpy.wait(5000));
    QCOMPARE(syncSpy.takeFirst().at(0).toBool(), true);
    QCOMPARE(m_client->pendingCount(), 0); // file vidée après acquittement

    // Vérité serveur : 25 en serre, 5 en zone de vente.
    int serreQty = -1, venteQty = -1;
    for (const StockLevel& level :
         m_stock->levelsOf(m_variantId).value()) {
        if (level.locationId == m_serre) serreQty = level.qty;
        if (level.locationId == m_vente) venteQty = level.qty;
    }
    QCOMPARE(serreQty, 25);
    QCOMPARE(venteQty, 5);

    // Réplique locale re-tirée = vérité serveur.
    const QVariantMap card = m_client->searchVariants(
        QStringLiteral("citron")).first().toMap();
    QCOMPARE(card.value(QStringLiteral("totalQty")).toInt(), 30);
    const QVariantList levels =
        card.value(QStringLiteral("levels")).toList();
    QCOMPARE(levels.size(), 2);
}

void TestMobileSync::inventoryAdjustFlow()
{
    // Comptage terrain en zone de vente : théorique local 5, compté 3
    // -> ajustement −2 dans la file, poussé, serveur à 3.
    const QVariantList lines = m_store->inventoryLines(m_vente);
    QCOMPARE(lines.size(), 1);
    QCOMPARE(lines.first().toMap().value(QStringLiteral("expected")).toInt(),
             5);

    QSignalSpy syncSpy(m_client, &SyncClient::syncFinished);
    QVERIFY(m_client->queueMove(QVariantMap{
        {QStringLiteral("kind"), QStringLiteral("adjust")},
        {QStringLiteral("variantId"), m_variantId},
        {QStringLiteral("fromId"), m_vente}, // écart négatif : from = −
        {QStringLiteral("qty"), 2},
        {QStringLiteral("note"), QStringLiteral("inventaire terrain")},
    }));
    QVERIFY(syncSpy.wait(5000));
    QCOMPARE(syncSpy.takeFirst().at(0).toBool(), true);

    int venteQty = -1;
    for (const StockLevel& level : m_stock->levelsOf(m_variantId).value())
        if (level.locationId == m_vente)
            venteQty = level.qty;
    QCOMPARE(venteQty, 3);
    QCOMPARE(m_client->pendingCount(), 0);
}

void TestMobileSync::photoReplication()
{
    // Photo principale posée côté poste : répliquée en fichier local à
    // la sync suivante, exposée par la recherche (photoUrl).
    const QByteArray fakeJpeg = QByteArrayLiteral("fake-jpeg-bytes-1234");
    const QString photoPath = m_dir.filePath(QStringLiteral("citron.jpg"));
    {
        QFile file(photoPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(fakeJpeg);
    }
    SqliteProductRepository products(m_db->connectionName());
    const int productId =
        products.search(QStringLiteral("Citron")).value().first().id;
    QVERIFY(products.setMainPhoto(productId, photoPath).isOk());

    QSignalSpy syncSpy(m_client, &SyncClient::syncFinished);
    m_client->syncNow();
    QVERIFY(syncSpy.wait(5000));
    QCOMPARE(syncSpy.takeFirst().at(0).toBool(), true);

    const QString local = m_store->photoPath(productId);
    QVERIFY(!local.isEmpty());
    QFile replicated(local);
    QVERIFY(replicated.open(QIODevice::ReadOnly));
    QCOMPARE(replicated.readAll(), fakeJpeg);

    const QVariantMap card = m_client->searchVariants(
        QStringLiteral("citron")).first().toMap();
    QVERIFY(card.value(QStringLiteral("photoUrl")).toString()
                .startsWith(QStringLiteral("file:")));
}

QTEST_GUILESS_MAIN(TestMobileSync)
#include "tst_mobile_sync.moc"
