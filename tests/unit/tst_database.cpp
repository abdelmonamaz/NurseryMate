#include "database/database_manager.h"

#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void migrationsApply();
    void openIsIdempotent();
};

void TestDatabase::migrationsApply()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    nursera::DatabaseManager db(dir.filePath(QStringLiteral("test.db")),
                                QStringLiteral("tst_db_migrations"));
    const auto opened = db.open();
    QVERIFY2(opened.isOk(), qPrintable(opened.isOk() ? QString() : opened.error().message));
    QCOMPARE(db.schemaVersion(), 20);

    QSqlQuery query(db.database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN "
        "('users', 'products', 'variants', 'locations', 'stock', 'stock_moves', "
        "'settings', 'doc_counters', 'audit_log', 'devices', 'categories', "
        "'product_photos')")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 12);

    // Clés étrangères actives (doc 02 §3.2)
    QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
}

void TestDatabase::openIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    nursera::DatabaseManager db(dir.filePath(QStringLiteral("test.db")),
                                QStringLiteral("tst_db_idempotent"));
    QVERIFY(db.open().isOk());
    // Une seconde ouverture ne doit pas rejouer les migrations ni échouer.
    QVERIFY(db.open().isOk());
    QCOMPARE(db.schemaVersion(), 20);
}

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
