#include "mobile_store.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUuid>
#include <QVariantMap>

#include <utility>

namespace nursera {

MobileStore::MobileStore(QString databasePath, QString connectionName)
    : m_databasePath(std::move(databasePath))
    , m_connectionName(std::move(connectionName))
{
}

MobileStore::~MobileStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

Result<void> MobileStore::open()
{
    QSqlDatabase database =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(m_databasePath);
    if (!database.open())
        return Result<void>::fail(QStringLiteral("mobile.open"),
                                  database.lastError().text());

    QSqlQuery query(database);
    const QStringList ddl = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta ("
                       "key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS products ("
                       "id INTEGER PRIMARY KEY, name_fr TEXT, name_ar TEXT, "
                       "type TEXT, category_id INTEGER, "
                       "photo INTEGER DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS variants ("
                       "id INTEGER PRIMARY KEY, product_id INTEGER, "
                       "packaging TEXT, sku TEXT, barcode TEXT, "
                       "price_ttc REAL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS locations ("
                       "id INTEGER PRIMARY KEY, name_fr TEXT, name_ar TEXT, "
                       "kind TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS stock ("
                       "variant_id INTEGER, location_id INTEGER, qty INTEGER, "
                       "PRIMARY KEY (variant_id, location_id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS pending_moves ("
                       "uuid TEXT PRIMARY KEY, kind TEXT, variant_id INTEGER, "
                       "from_id INTEGER, to_id INTEGER, qty INTEGER, "
                       "note TEXT, loss_reason TEXT, "
                       "created_at TEXT DEFAULT (datetime('now')))"),
    };
    for (const QString& statement : ddl)
        if (!query.exec(statement))
            return Result<void>::fail(QStringLiteral("mobile.schema"),
                                      query.lastError().text());
    return Result<void>::ok();
}

Result<void> MobileStore::replaceCatalog(const QJsonArray& products,
                                         const QJsonArray& variants,
                                         const QJsonArray& locations)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("mobile.tx"),
                                  database.lastError().text());
    QSqlQuery query(database);
    query.exec(QStringLiteral("DELETE FROM products"));
    query.exec(QStringLiteral("DELETE FROM variants"));
    query.exec(QStringLiteral("DELETE FROM locations"));

    for (const QJsonValue& value : products) {
        const QJsonObject row = value.toObject();
        query.prepare(QStringLiteral(
            "INSERT INTO products (id, name_fr, name_ar, type, category_id, "
            "photo) VALUES (?, ?, ?, ?, ?, ?)"));
        query.addBindValue(row.value(QStringLiteral("id")).toInt());
        query.addBindValue(row.value(QStringLiteral("nameFr")).toString());
        query.addBindValue(row.value(QStringLiteral("nameAr")).toString());
        query.addBindValue(row.value(QStringLiteral("type")).toString());
        query.addBindValue(row.value(QStringLiteral("categoryId")).toInt());
        query.addBindValue(row.value(QStringLiteral("photo")).toBool() ? 1 : 0);
        if (!query.exec()) {
            database.rollback();
            return Result<void>::fail(QStringLiteral("mobile.products"),
                                      query.lastError().text());
        }
    }
    for (const QJsonValue& value : variants) {
        const QJsonObject row = value.toObject();
        query.prepare(QStringLiteral(
            "INSERT INTO variants (id, product_id, packaging, sku, barcode, "
            "price_ttc) VALUES (?, ?, ?, ?, ?, ?)"));
        query.addBindValue(row.value(QStringLiteral("id")).toInt());
        query.addBindValue(row.value(QStringLiteral("productId")).toInt());
        query.addBindValue(row.value(QStringLiteral("packaging")).toString());
        query.addBindValue(row.value(QStringLiteral("sku")).toString());
        query.addBindValue(row.value(QStringLiteral("barcode")).toString());
        query.addBindValue(row.value(QStringLiteral("priceTtc")).toDouble());
        if (!query.exec()) {
            database.rollback();
            return Result<void>::fail(QStringLiteral("mobile.variants"),
                                      query.lastError().text());
        }
    }
    for (const QJsonValue& value : locations) {
        const QJsonObject row = value.toObject();
        query.prepare(QStringLiteral(
            "INSERT INTO locations (id, name_fr, name_ar, kind) "
            "VALUES (?, ?, ?, ?)"));
        query.addBindValue(row.value(QStringLiteral("id")).toInt());
        query.addBindValue(row.value(QStringLiteral("nameFr")).toString());
        query.addBindValue(row.value(QStringLiteral("nameAr")).toString());
        query.addBindValue(row.value(QStringLiteral("kind")).toString());
        if (!query.exec()) {
            database.rollback();
            return Result<void>::fail(QStringLiteral("mobile.locations"),
                                      query.lastError().text());
        }
    }
    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("mobile.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<void> MobileStore::replaceStock(const QJsonArray& levels)
{
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<void>::fail(QStringLiteral("mobile.tx"),
                                  database.lastError().text());
    QSqlQuery query(database);
    query.exec(QStringLiteral("DELETE FROM stock"));
    for (const QJsonValue& value : levels) {
        const QJsonObject row = value.toObject();
        query.prepare(QStringLiteral(
            "INSERT INTO stock (variant_id, location_id, qty) "
            "VALUES (?, ?, ?)"));
        query.addBindValue(row.value(QStringLiteral("variantId")).toInt());
        query.addBindValue(row.value(QStringLiteral("locationId")).toInt());
        query.addBindValue(row.value(QStringLiteral("qty")).toInt());
        if (!query.exec()) {
            database.rollback();
            return Result<void>::fail(QStringLiteral("mobile.stock"),
                                      query.lastError().text());
        }
    }
    if (!database.commit()) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("mobile.commit"),
                                  database.lastError().text());
    }
    return Result<void>::ok();
}

Result<QString> MobileStore::queueMove(const QString& kind, int variantId,
                                       int fromId, int toId, int qty,
                                       const QString& note,
                                       const QString& lossReason)
{
    if (variantId <= 0 || qty <= 0)
        return Result<QString>::fail(QStringLiteral("mobile.move"),
                                     QStringLiteral("Produit et quantité "
                                                    "obligatoires."));
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase database = db();
    if (!database.transaction())
        return Result<QString>::fail(QStringLiteral("mobile.tx"),
                                     database.lastError().text());
    QSqlQuery insert(database);
    insert.prepare(QStringLiteral(
        "INSERT INTO pending_moves (uuid, kind, variant_id, from_id, to_id, "
        "qty, note, loss_reason) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(uuid);
    insert.addBindValue(kind);
    insert.addBindValue(variantId);
    insert.addBindValue(fromId);
    insert.addBindValue(toId);
    insert.addBindValue(qty);
    insert.addBindValue(note);
    insert.addBindValue(lossReason);
    if (!insert.exec()) {
        database.rollback();
        return Result<QString>::fail(QStringLiteral("mobile.queue"),
                                     insert.lastError().text());
    }

    // Effet local immédiat (optimiste) : l'ouvrier voit son geste tout de
    // suite ; la prochaine sync remplace par la vérité du serveur.
    const auto applyDelta = [&database](int variant, int location, int delta) {
        if (location <= 0)
            return true;
        QSqlQuery upsert(database);
        upsert.prepare(QStringLiteral(
            "INSERT INTO stock (variant_id, location_id, qty) "
            "VALUES (?, ?, ?) "
            "ON CONFLICT (variant_id, location_id) "
            "DO UPDATE SET qty = qty + excluded.qty"));
        upsert.addBindValue(variant);
        upsert.addBindValue(location);
        upsert.addBindValue(delta);
        return upsert.exec();
    };
    bool applied = true;
    if (kind == QLatin1String("out"))
        applied = applyDelta(variantId, fromId, -qty);
    else if (kind == QLatin1String("in"))
        applied = applyDelta(variantId, toId, qty);
    else if (kind == QLatin1String("transfer"))
        applied = applyDelta(variantId, fromId, -qty)
            && applyDelta(variantId, toId, qty);
    else if (kind == QLatin1String("adjust"))
        // Écart d'inventaire : to = +, from = − (sémantique adjust)
        applied = toId > 0 ? applyDelta(variantId, toId, qty)
                           : applyDelta(variantId, fromId, -qty);
    if (!applied) {
        database.rollback();
        return Result<QString>::fail(QStringLiteral("mobile.apply"),
                                     QStringLiteral("Mise à jour locale "
                                                    "impossible."));
    }
    if (!database.commit()) {
        database.rollback();
        return Result<QString>::fail(QStringLiteral("mobile.commit"),
                                     database.lastError().text());
    }
    return Result<QString>::ok(uuid);
}

QVariantList MobileStore::pendingMoves() const
{
    QVariantList rows;
    QSqlQuery query(db());
    query.exec(QStringLiteral(
        "SELECT uuid, kind, variant_id, from_id, to_id, qty, note, "
        "loss_reason FROM pending_moves ORDER BY created_at"));
    while (query.next())
        rows.append(QVariantMap{
            {QStringLiteral("uuid"), query.value(0).toString()},
            {QStringLiteral("kind"), query.value(1).toString()},
            {QStringLiteral("variantId"), query.value(2).toInt()},
            {QStringLiteral("fromId"), query.value(3).toInt()},
            {QStringLiteral("toId"), query.value(4).toInt()},
            {QStringLiteral("qty"), query.value(5).toInt()},
            {QStringLiteral("note"), query.value(6).toString()},
            {QStringLiteral("lossReason"), query.value(7).toString()},
        });
    return rows;
}

int MobileStore::pendingCount() const
{
    QSqlQuery query(db());
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM pending_moves"))
        && query.next())
        return query.value(0).toInt();
    return 0;
}

Result<void> MobileStore::removePending(const QString& uuid)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("DELETE FROM pending_moves WHERE uuid = ?"));
    query.addBindValue(uuid);
    if (!query.exec())
        return Result<void>::fail(QStringLiteral("mobile.removePending"),
                                  query.lastError().text());
    return Result<void>::ok();
}

QVariantList MobileStore::searchVariants(const QString& term, int limit) const
{
    QVariantList rows;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT v.id, p.name_fr, COALESCE(p.name_ar, ''), v.packaging, "
        "       v.sku, COALESCE((SELECT SUM(s.qty) FROM stock s "
        "                        WHERE s.variant_id = v.id), 0), p.id "
        "FROM variants v JOIN products p ON p.id = v.product_id "
        "WHERE p.name_fr LIKE ? OR p.name_ar LIKE ? OR v.sku LIKE ? "
        "   OR v.barcode = ? "
        "ORDER BY p.name_fr COLLATE NOCASE, v.packaging LIMIT ?"));
    const QString like = QLatin1Char('%') + term + QLatin1Char('%');
    query.addBindValue(like);
    query.addBindValue(like);
    query.addBindValue(like);
    query.addBindValue(term);
    query.addBindValue(limit);
    if (!query.exec())
        return rows;
    while (query.next()) {
        const int variantId = query.value(0).toInt();
        // Détail par emplacement pour la fiche terrain
        QVariantList perLocation;
        QSqlQuery levels(db());
        levels.prepare(QStringLiteral(
            "SELECT l.name_fr, s.qty FROM stock s "
            "JOIN locations l ON l.id = s.location_id "
            "WHERE s.variant_id = ? AND s.qty != 0 ORDER BY l.id"));
        levels.addBindValue(variantId);
        if (levels.exec())
            while (levels.next())
                perLocation.append(QVariantMap{
                    {QStringLiteral("location"), levels.value(0).toString()},
                    {QStringLiteral("qty"), levels.value(1).toInt()},
                });
        const QString photo = photoPath(query.value(6).toInt());
        rows.append(QVariantMap{
            {QStringLiteral("variantId"), variantId},
            {QStringLiteral("label"),
             QStringLiteral("%1 — %2").arg(query.value(1).toString(),
                                           query.value(3).toString())},
            {QStringLiteral("nameAr"), query.value(2).toString()},
            {QStringLiteral("sku"), query.value(4).toString()},
            {QStringLiteral("totalQty"), query.value(5).toInt()},
            {QStringLiteral("levels"), perLocation},
            {QStringLiteral("photoUrl"),
             photo.isEmpty() ? QString()
                             : QUrl::fromLocalFile(photo).toString()},
        });
    }
    return rows;
}

QVariantList MobileStore::locationOptions() const
{
    QVariantList rows;
    QSqlQuery query(db());
    query.exec(QStringLiteral(
        "SELECT id, name_fr FROM locations ORDER BY id"));
    while (query.next())
        rows.append(QVariantMap{
            {QStringLiteral("id"), query.value(0).toInt()},
            {QStringLiteral("label"), query.value(1).toString()},
        });
    return rows;
}

QVariantList MobileStore::inventoryLines(int locationId) const
{
    QVariantList rows;
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "SELECT v.id, p.name_fr, COALESCE(p.name_ar, ''), v.packaging, "
        "       v.sku, s.qty "
        "FROM stock s "
        "JOIN variants v ON v.id = s.variant_id "
        "JOIN products p ON p.id = v.product_id "
        "WHERE s.location_id = ? "
        "ORDER BY p.name_fr COLLATE NOCASE, v.packaging"));
    query.addBindValue(locationId);
    if (!query.exec())
        return rows;
    while (query.next())
        rows.append(QVariantMap{
            {QStringLiteral("variantId"), query.value(0).toInt()},
            {QStringLiteral("label"),
             QStringLiteral("%1 — %2").arg(query.value(1).toString(),
                                           query.value(3).toString())},
            {QStringLiteral("nameAr"), query.value(2).toString()},
            {QStringLiteral("sku"), query.value(4).toString()},
            {QStringLiteral("expected"), query.value(5).toInt()},
        });
    return rows;
}

QString MobileStore::photosDir() const
{
    return QFileInfo(m_databasePath).absolutePath()
        + QStringLiteral("/photos");
}

QString MobileStore::photoPath(int productId) const
{
    const QString path =
        photosDir() + QStringLiteral("/%1.jpg").arg(productId);
    return QFile::exists(path) ? path : QString();
}

QList<int> MobileStore::productsMissingPhoto() const
{
    QList<int> missing;
    QSqlQuery query(db());
    query.exec(QStringLiteral("SELECT id FROM products WHERE photo = 1"));
    while (query.next()) {
        const int productId = query.value(0).toInt();
        if (photoPath(productId).isEmpty())
            missing.append(productId);
    }
    return missing;
}

QString MobileStore::metaValue(const QString& key,
                               const QString& fallback) const
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
    query.addBindValue(key);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return fallback;
}

void MobileStore::setMetaValue(const QString& key, const QString& value)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO meta (key, value) VALUES (?, ?) "
        "ON CONFLICT (key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

} // namespace nursera
