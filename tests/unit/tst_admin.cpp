#include "common/password_hasher.h"
#include "controllers/admin_controller.h"
#include "database/backup_service.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_category_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_settings_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_user_repository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestAdmin : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void backupCreatesValidFile();
    void autoBackupOncePerDay();
    void rotationRemovesOldBackups();
    void userManagement();
    void lastManagerProtected();
    void categoryReferentials();
    void locationReferentials();
    void documentNumbering();
    void guidedRestore();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteUserRepository* m_users = nullptr;
    SqliteSettingsRepository* m_settings = nullptr;
    BackupService* m_backups = nullptr;
    SqliteCategoryRepository* m_categories = nullptr;
    SqliteLocationRepository* m_locations = nullptr;
    AdminController* m_admin = nullptr;
};

void TestAdmin::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("admin.db")),
                               QStringLiteral("tst_admin"));
    QVERIFY(m_db->open().isOk());

    m_users = new SqliteUserRepository(m_db->connectionName());
    m_settings = new SqliteSettingsRepository(m_db->connectionName());
    m_backups = new BackupService(m_db->connectionName(),
                                  m_dir.filePath(QStringLiteral("backups")));
    m_categories = new SqliteCategoryRepository(m_db->connectionName());
    m_locations = new SqliteLocationRepository(m_db->connectionName());
    m_admin = new AdminController(*m_users, *m_settings, *m_backups);
    m_admin->configureReferentials(m_categories, m_locations);
    m_admin->refresh();
}

void TestAdmin::backupCreatesValidFile()
{
    const auto backup = m_backups->backupNow();
    QVERIFY2(backup.isOk(),
             qPrintable(backup.isOk() ? QString() : backup.error().message));

    const QFileInfo file(backup.value());
    QVERIFY(file.exists());
    QVERIFY(file.size() > 0);
    QVERIFY(file.fileName().startsWith(QStringLiteral("nursera-")));

    // Le fichier produit est une base SQLite lisible avec le bon schéma
    DatabaseManager check(backup.value(), QStringLiteral("tst_admin_check"));
    QVERIFY(check.open().isOk());
    QCOMPARE(check.schemaVersion(), 20);

    QCOMPARE(m_backups->list().size(), 1);
}

void TestAdmin::autoBackupOncePerDay()
{
    // Première fois du jour : crée un fichier
    const auto first = m_backups->autoBackupIfDue(*m_settings);
    QVERIFY(first.isOk());
    QVERIFY(!first.value().isEmpty());

    // Deuxième appel le même jour : non due, aucun fichier
    const int count = m_backups->list().size();
    const auto second = m_backups->autoBackupIfDue(*m_settings);
    QVERIFY(second.isOk());
    QVERIFY(second.value().isEmpty());
    QCOMPARE(m_backups->list().size(), count);
}

void TestAdmin::rotationRemovesOldBackups()
{
    // Une vieille sauvegarde (> 30 jours au nom) disparaît à la rotation
    const QString oldBackup = m_backups->backupDir()
        + QStringLiteral("/nursera-20200101-000000.db");
    {
        QFile file(oldBackup);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("obsolete");
    }
    QVERIFY(QFile::exists(oldBackup));

    QVERIFY(m_backups->backupNow().isOk()); // déclenche la rotation
    QVERIFY(!QFile::exists(oldBackup));
}

void TestAdmin::userManagement()
{
    // Création du Gérant puis d'une Vendeuse
    QVERIFY(m_admin->createUser({{QStringLiteral("name"), QStringLiteral("Sami")},
                                 {QStringLiteral("role"), QStringLiteral("manager")},
                                 {QStringLiteral("pin"), QStringLiteral("1234")},
                                 {QStringLiteral("confirm"), QStringLiteral("1234")}}));
    QVERIFY(m_admin->createUser({{QStringLiteral("name"), QStringLiteral("Leïla")},
                                 {QStringLiteral("role"), QStringLiteral("seller")},
                                 {QStringLiteral("pin"), QStringLiteral("2345")},
                                 {QStringLiteral("confirm"), QStringLiteral("2345")}}));
    QCOMPARE(m_admin->users().size(), 2);

    // Validations
    QVERIFY(!m_admin->createUser({{QStringLiteral("name"), QStringLiteral("X")},
                                  {QStringLiteral("pin"), QStringLiteral("12")},
                                  {QStringLiteral("confirm"), QStringLiteral("12")}}));
    QVERIFY(!m_admin->createUser({{QStringLiteral("name"), QStringLiteral("X")},
                                  {QStringLiteral("pin"), QStringLiteral("1234")},
                                  {QStringLiteral("confirm"), QStringLiteral("9999")}}));

    // Réinitialisation du PIN, vérifiée par le hachage stocké
    const int leilaId = m_admin->users().last().toMap()
                            .value(QStringLiteral("id")).toInt();
    QVERIFY(m_admin->resetPin(leilaId, QStringLiteral("777777"),
                              QStringLiteral("777777")));
    QVERIFY(PasswordHasher::verify(QStringLiteral("777777"),
                                   m_users->pinHashOf(leilaId).value()));
    QVERIFY(!PasswordHasher::verify(QStringLiteral("2345"),
                                    m_users->pinHashOf(leilaId).value()));

    // Désactivation de la Vendeuse : OK (RG-01.b : conservée, pas supprimée)
    // — elle reste listée (badge Inactif), triée après les actifs.
    QVERIFY(m_admin->deactivateUser(leilaId));
    QCOMPARE(m_admin->users().size(), 2);
    QCOMPARE(m_admin->users().last().toMap()
                 .value(QStringLiteral("active")).toBool(), false);
    QCOMPARE(m_users->totalCount().value(), 2);

    // Réversible : la réactivation la remet en service.
    QVERIFY(m_admin->reactivateUser(leilaId));
    QCOMPARE(m_admin->users().last().toMap()
                 .value(QStringLiteral("active")).toBool(), true);
    // Re-désactivée pour le test suivant (dernier gérant).
    QVERIFY(m_admin->deactivateUser(leilaId));
}

void TestAdmin::lastManagerProtected()
{
    // RG-01.a : le dernier Gérant actif ne peut pas être désactivé
    const int samiId = m_admin->users().first().toMap()
                           .value(QStringLiteral("id")).toInt();
    QVERIFY(!m_admin->deactivateUser(samiId));
    QCOMPARE(m_admin->users().first().toMap()
                 .value(QStringLiteral("active")).toBool(), true);
}

void TestAdmin::categoryReferentials()
{
    const int seeded = m_admin->categories().size();
    QVERIFY(seeded > 0); // migrations 002/009

    // Création
    QVERIFY(m_admin->saveCategory({{QStringLiteral("nameFr"), QStringLiteral("Cactées")},
                                   {QStringLiteral("nameAr"), QStringLiteral("صبّار")},
                                   {QStringLiteral("sortOrder"), 99}}));
    QCOMPARE(m_admin->categories().size(), seeded + 1);
    QVERIFY(!m_admin->saveCategory({{QStringLiteral("nameFr"), QStringLiteral("  ")}}));

    int cactusId = 0;
    for (const QVariant& row : m_admin->categories())
        if (row.toMap().value(QStringLiteral("nameFr")).toString()
            == QStringLiteral("Cactées"))
            cactusId = row.toMap().value(QStringLiteral("id")).toInt();
    QVERIFY(cactusId > 0);

    // Édition (renommage) — le statut est conservé
    QVERIFY(m_admin->saveCategory({{QStringLiteral("id"), cactusId},
                                   {QStringLiteral("nameFr"),
                                    QStringLiteral("Cactées et succulentes")},
                                   {QStringLiteral("sortOrder"), 99}}));

    // Désactivation réversible
    QVERIFY(m_admin->setCategoryActive(cactusId, false));
    QCOMPARE(m_categories->all(false).value().size(), seeded);
    QVERIFY(m_admin->setCategoryActive(cactusId, true));

    // Jamais référencée -> supprimable ; référencée -> refus (norme)
    QVERIFY(m_admin->categoryDeletable(cactusId));
    SqliteProductRepository products(m_db->connectionName());
    Product aloe;
    aloe.nameFr = QStringLiteral("Aloe vera");
    aloe.categoryId = cactusId;
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(8000);
    QVERIFY(products.insertWithVariants(aloe, {godet}).isOk());
    QVERIFY(!m_admin->categoryDeletable(cactusId));
    QVERIFY(!m_admin->deleteCategory(cactusId));
    QCOMPARE(m_admin->categories().size(), seeded + 1);

    // Une catégorie jamais utilisée se supprime
    QVERIFY(m_admin->saveCategory({{QStringLiteral("nameFr"), QStringLiteral("Éphémère")}}));
    int tempId = 0;
    for (const QVariant& row : m_admin->categories())
        if (row.toMap().value(QStringLiteral("nameFr")).toString()
            == QStringLiteral("Éphémère"))
            tempId = row.toMap().value(QStringLiteral("id")).toInt();
    QVERIFY(m_admin->deleteCategory(tempId));
    QCOMPARE(m_admin->categories().size(), seeded + 1);
}

void TestAdmin::locationReferentials()
{
    const int seeded = m_admin->locations().size();
    QVERIFY(seeded >= 3); // migration 003 : Serre 1, Zone de vente, Dépôt

    // Création + suppression (jamais référencé)
    QVERIFY(m_admin->saveLocation({{QStringLiteral("nameFr"), QStringLiteral("Serre 2")},
                                   {QStringLiteral("kind"), QStringLiteral("greenhouse")}}));
    QCOMPARE(m_admin->locations().size(), seeded + 1);
    int serre2Id = 0, serre1Id = 0, venteId = 0;
    for (const QVariant& row : m_admin->locations()) {
        const QVariantMap map = row.toMap();
        if (map.value(QStringLiteral("nameFr")) == QLatin1String("Serre 2"))
            serre2Id = map.value(QStringLiteral("id")).toInt();
        if (map.value(QStringLiteral("nameFr")) == QLatin1String("Serre 1"))
            serre1Id = map.value(QStringLiteral("id")).toInt();
        if (map.value(QStringLiteral("kind")) == QLatin1String("sales_area"))
            venteId = map.value(QStringLiteral("id")).toInt();
    }
    QVERIFY(serre2Id > 0 && serre1Id > 0 && venteId > 0);
    QVERIFY(m_admin->locationDeletable(serre2Id));
    QVERIFY(m_admin->deleteLocation(serre2Id));
    QCOMPARE(m_admin->locations().size(), seeded);

    // La caisse exige une zone de vente active : ni désactivation ni
    // suppression ni changement de type de la dernière (RG-04.a).
    QVERIFY(!m_admin->setLocationActive(venteId, false));
    QVERIFY(!m_admin->deleteLocation(venteId));
    QVERIFY(!m_admin->saveLocation({{QStringLiteral("id"), venteId},
                                    {QStringLiteral("nameFr"), QStringLiteral("Zone de vente")},
                                    {QStringLiteral("kind"), QStringLiteral("warehouse")}}));

    // Un emplacement avec mouvement n'est plus supprimable, mais reste
    // désactivable (réversible).
    SqliteStockRepository stock(m_db->connectionName());
    SqliteProductRepository products(m_db->connectionName());
    const auto found = products.search(QStringLiteral("Aloe"));
    QVERIFY(found.isOk() && !found.value().isEmpty());
    const int aloeId = found.value().first().id;
    const auto variants = products.variantsOf(aloeId);
    QVERIFY(variants.isOk() && !variants.value().isEmpty());
    const int variantId = variants.value().first().id;
    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = serre1Id;
    entry.variantId = variantId;
    entry.qty = 5;
    QVERIFY(stock.recordMove(entry).isOk());
    QVERIFY(!m_admin->locationDeletable(serre1Id));
    QVERIFY(!m_admin->deleteLocation(serre1Id));
    QVERIFY(m_admin->setLocationActive(serre1Id, false));
    QVERIFY(m_admin->setLocationActive(serre1Id, true));
}

void TestAdmin::documentNumbering()
{
    // F11-07 : base vierge -> tous les compteurs affichent le n° 1,
    // au format de chaque document.
    m_admin->refresh();
    const QVariantList numbers = m_admin->docNumbers();
    QCOMPARE(numbers.size(), 6);
    const int year = QDate::currentDate().year();
    QCOMPARE(numbers.first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Ticket de caisse"));
    QCOMPARE(numbers.first().toMap().value(QStringLiteral("number")).toString(),
             QStringLiteral("T-%1-00001").arg(year));
    QCOMPARE(numbers.at(2).toMap().value(QStringLiteral("number")).toString(),
             QStringLiteral("AV-%1-001").arg(year));

    QVERIFY(m_settings->documentCounters(year).isOk());
    QVERIFY(m_settings->documentCounters(year).value().isEmpty());
}

void TestAdmin::guidedRestore()
{
    // Mise en attente : la sauvegarde (intégrité vérifiée) est copiée en
    // restore-pending.db à côté de la base — la base ouverte n'est pas touchée.
    const QString backupName = m_backups->list().first().fileName;
    QVERIFY(!m_backups->restorePending());
    QVERIFY2(m_backups->stageRestore(backupName).isOk(), "stage");
    QVERIFY(m_backups->restorePending());
    QVERIFY(m_admin->restorePending());

    // Annulable tant que l'app n'a pas redémarré.
    m_backups->cancelPendingRestore();
    QVERIFY(!m_backups->restorePending());
    QVERIFY(!m_backups->stageRestore(QStringLiteral("inexistante.db")).isOk());

    // Application au démarrage (base fermée) — scénario autonome :
    // une « base » + une restauration en attente dans un dossier à part.
    const QString dir = m_dir.filePath(QStringLiteral("standalone"));
    QVERIFY(QDir().mkpath(dir));
    const QString dbPath = dir + QStringLiteral("/nursera.db");
    const QString backupPath = m_backups->list().first().filePath;
    QVERIFY(QFile::copy(backupPath, dbPath));
    QVERIFY(QFile::copy(backupPath, dir + QStringLiteral("/restore-pending.db")));

    const auto applied = BackupService::applyPendingRestore(dbPath);
    QVERIFY2(applied.isOk(), qPrintable(applied.isOk()
        ? QString() : applied.error().message));
    QVERIFY(applied.value());
    QVERIFY(QFile::exists(dbPath));
    QVERIFY(!QFile::exists(dir + QStringLiteral("/restore-pending.db")));
    // Filet de sécurité : l'ancienne base est conservée en pre-restore-….db
    QCOMPARE(QDir(dir).entryList({QStringLiteral("pre-restore-*.db")},
                                 QDir::Files).size(), 1);
    // Rien en attente -> aucun effet
    QVERIFY(!BackupService::applyPendingRestore(dbPath).value());
}

QTEST_GUILESS_MAIN(TestAdmin)
#include "tst_admin.moc"
