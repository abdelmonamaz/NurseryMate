#include "common/password_hasher.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_user_repository.h"
#include "sync_server.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

// Le serveur LAN de sync (M09) testé de bout en bout en HTTP réel :
// ping -> auth -> catalogue -> push de mouvements idempotents.
class TestSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pingWithoutAuth();
    void authAndToken();
    void catalogRequiresToken();
    void pushMovesIdempotent();

private:
    QJsonObject request(const QString& method, const QString& path,
                        const QJsonObject& body = {},
                        const QString& token = {},
                        int* statusCode = nullptr);

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteUserRepository* m_users = nullptr;
    SyncServer* m_server = nullptr;
    QNetworkAccessManager m_network;
    quint16 m_port = 0;
    QString m_token;
    int m_variantId = 0;
    int m_serre = 0;
};

QJsonObject TestSync::request(const QString& method, const QString& path,
                              const QJsonObject& body, const QString& token,
                              int* statusCode)
{
    QNetworkRequest networkRequest(
        QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_port).arg(path)));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    if (!token.isEmpty())
        networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                    QByteArrayLiteral("Bearer ")
                                        + token.toUtf8());

    QNetworkReply* reply = method == QLatin1String("POST")
        ? m_network.post(networkRequest,
                         QJsonDocument(body).toJson(QJsonDocument::Compact))
        : m_network.get(networkRequest);

    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    if (statusCode)
        *statusCode = reply->attribute(
                              QNetworkRequest::HttpStatusCodeAttribute)
                          .toInt();
    const QJsonObject result =
        QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();
    return result;
}

void TestSync::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("sync.db")),
                               QStringLiteral("tst_sync"));
    QVERIFY(m_db->open().isOk());

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_users = new SqliteUserRepository(m_db->connectionName());

    // Utilisatrice terrain + produit + stock initial
    User leila;
    leila.username = QStringLiteral("leila");
    leila.displayName = QStringLiteral("Leïla");
    leila.role = UserRole::Worker;
    QVERIFY(m_users->insert(leila, PasswordHasher::hash(
                                       QStringLiteral("2345"))).isOk());

    SqliteLocationRepository locations(m_db->connectionName());
    m_serre = locations.all().value().first().id;

    SqliteProductRepository products(m_db->connectionName());
    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    const int productId =
        products.insertWithVariants(romarin, {godet}).value();
    m_variantId = products.variantsOf(productId).value().first().id;

    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_serre;
    entry.variantId = m_variantId;
    entry.qty = 10;
    QVERIFY(m_stock->recordMove(entry).isOk());

    // Serveur sur un port libre choisi par l'OS
    m_server = new SyncServer(m_db->connectionName(), *m_users, *m_stock);
    const auto started = m_server->start(0);
    QVERIFY2(started.isOk(),
             qPrintable(started.isOk() ? QString()
                                       : started.error().message));
    m_port = started.value();
    QVERIFY(m_port > 0);
}

void TestSync::pingWithoutAuth()
{
    int status = 0;
    const QJsonObject ping = request(QStringLiteral("GET"),
                                     QStringLiteral("/api/v1/ping"), {},
                                     {}, &status);
    QCOMPARE(status, 200);
    QCOMPARE(ping.value(QStringLiteral("app")).toString(),
             QStringLiteral("nursera"));
    QCOMPARE(ping.value(QStringLiteral("schemaVersion")).toInt(), 20);
}

void TestSync::authAndToken()
{
    // Mauvais PIN -> 401
    int status = 0;
    request(QStringLiteral("POST"), QStringLiteral("/api/v1/auth"),
            QJsonObject{{QStringLiteral("username"), QStringLiteral("leila")},
                        {QStringLiteral("pin"), QStringLiteral("9999")}},
            {}, &status);
    QCOMPARE(status, 401);

    // Bon PIN -> jeton
    const QJsonObject auth = request(
        QStringLiteral("POST"), QStringLiteral("/api/v1/auth"),
        QJsonObject{{QStringLiteral("username"), QStringLiteral("leila")},
                    {QStringLiteral("pin"), QStringLiteral("2345")}},
        {}, &status);
    QCOMPARE(status, 200);
    m_token = auth.value(QStringLiteral("token")).toString();
    QVERIFY(m_token.length() > 40);
    QCOMPARE(auth.value(QStringLiteral("role")).toString(),
             QStringLiteral("worker"));
}

void TestSync::catalogRequiresToken()
{
    int status = 0;
    request(QStringLiteral("GET"), QStringLiteral("/api/v1/catalog"), {},
            {}, &status);
    QCOMPARE(status, 401);

    const QJsonObject catalog = request(QStringLiteral("GET"),
                                        QStringLiteral("/api/v1/catalog"),
                                        {}, m_token, &status);
    QCOMPARE(status, 200);
    QCOMPARE(catalog.value(QStringLiteral("products")).toArray().size(), 1);
    QCOMPARE(catalog.value(QStringLiteral("variants")).toArray().size(), 1);
    QVERIFY(catalog.value(QStringLiteral("locations")).toArray().size() >= 3);

    const QJsonObject stock = request(QStringLiteral("GET"),
                                      QStringLiteral("/api/v1/stock"),
                                      {}, m_token, &status);
    QCOMPARE(status, 200);
    QCOMPARE(stock.value(QStringLiteral("levels")).toArray().first()
                 .toObject().value(QStringLiteral("qty")).toInt(), 10);
}

void TestSync::pushMovesIdempotent()
{
    // Sortie terrain de 2 — puis REJEU du même uuid (coupure réseau) :
    // appliquée UNE seule fois.
    const QString uuid = QStringLiteral("11111111-2222-3333-4444-555555555555");
    const QJsonObject payload{{QStringLiteral("moves"), QJsonArray{
        QJsonObject{{QStringLiteral("uuid"), uuid},
                    {QStringLiteral("kind"), QStringLiteral("out")},
                    {QStringLiteral("variantId"), m_variantId},
                    {QStringLiteral("fromId"), m_serre},
                    {QStringLiteral("qty"), 2},
                    {QStringLiteral("note"), QStringLiteral("casse terrain")}},
    }}};

    int status = 0;
    QJsonObject reply = request(QStringLiteral("POST"),
                                QStringLiteral("/api/v1/moves"), payload,
                                m_token, &status);
    QCOMPARE(status, 200);
    QCOMPARE(reply.value(QStringLiteral("results")).toArray().first()
                 .toObject().value(QStringLiteral("status")).toString(),
             QStringLiteral("ok"));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 8);

    reply = request(QStringLiteral("POST"), QStringLiteral("/api/v1/moves"),
                    payload, m_token, &status);
    QCOMPARE(reply.value(QStringLiteral("results")).toArray().first()
                 .toObject().value(QStringLiteral("status")).toString(),
             QStringLiteral("ok"));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 8); // 1 fois

    // uuid manquant -> refusé (idempotence impossible)
    const QJsonObject bad{{QStringLiteral("moves"), QJsonArray{
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("out")},
                    {QStringLiteral("variantId"), m_variantId},
                    {QStringLiteral("fromId"), m_serre},
                    {QStringLiteral("qty"), 1}},
    }}};
    reply = request(QStringLiteral("POST"), QStringLiteral("/api/v1/moves"),
                    bad, m_token, &status);
    QCOMPARE(reply.value(QStringLiteral("results")).toArray().first()
                 .toObject().value(QStringLiteral("status")).toString(),
             QStringLiteral("error"));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 8);
}

QTEST_GUILESS_MAIN(TestSync)
#include "tst_sync.moc"
