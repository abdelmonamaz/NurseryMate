#include "sync_server.h"

#include "common/password_hasher.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpSocket>
#include <QUuid>

#include <utility>

namespace nursera {
namespace {

constexpr int kTokenHours = 12;
constexpr qint64 kMaxRequestBytes = 4 * 1024 * 1024; // garde-fou LAN

QString roleCode(UserRole role)
{
    switch (role) {
    case UserRole::Manager: return QStringLiteral("manager");
    case UserRole::Seller: return QStringLiteral("seller");
    case UserRole::Worker: break;
    }
    return QStringLiteral("worker");
}

QString statusText(int status)
{
    switch (status) {
    case 200: return QStringLiteral("OK");
    case 400: return QStringLiteral("Bad Request");
    case 401: return QStringLiteral("Unauthorized");
    case 404: return QStringLiteral("Not Found");
    default: return QStringLiteral("Internal Server Error");
    }
}

} // namespace

SyncServer::SyncServer(QString connectionName, IUserRepository& users,
                       IStockRepository& stock, QObject* parent)
    : QObject(parent)
    , m_connectionName(std::move(connectionName))
    , m_users(users)
    , m_stock(stock)
{
}

QStringList SyncServer::localAddresses()
{
    QStringList addresses;
    for (const QHostAddress& address : QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol
            && !address.isLoopback())
            addresses.append(address.toString());
    }
    return addresses;
}

Result<quint16> SyncServer::start(quint16 port)
{
    if (!m_tcp.listen(QHostAddress::Any, port))
        return Result<quint16>::fail(QStringLiteral("sync.listen"),
                                     m_tcp.errorString());
    connect(&m_tcp, &QTcpServer::newConnection, this,
            &SyncServer::onNewConnection);
    m_port = m_tcp.serverPort();

    // Découverte LAN (F09-09) : réponse UDP au broadcast du mobile —
    // même port que le TCP. Échec non bloquant (le QR et la saisie
    // manuelle restent disponibles).
    if (m_discovery.bind(QHostAddress::AnyIPv4, m_port,
                         QUdpSocket::ShareAddress
                             | QUdpSocket::ReuseAddressHint)) {
        connect(&m_discovery, &QUdpSocket::readyRead, this, [this] {
            while (m_discovery.hasPendingDatagrams()) {
                QByteArray datagram;
                datagram.resize(int(m_discovery.pendingDatagramSize()));
                QHostAddress sender;
                quint16 senderPort = 0;
                m_discovery.readDatagram(datagram.data(), datagram.size(),
                                         &sender, &senderPort);
                if (!datagram.startsWith("nursera-discover"))
                    continue;
                const QByteArray reply =
                    QJsonDocument(QJsonObject{
                                      {QStringLiteral("app"),
                                       QStringLiteral("nursera")},
                                      {QStringLiteral("port"), m_port},
                                  })
                        .toJson(QJsonDocument::Compact);
                m_discovery.writeDatagram(reply, sender, senderPort);
            }
        });
    }
    return Result<quint16>::ok(m_port);
}

void SyncServer::onNewConnection()
{
    while (QTcpSocket* socket = m_tcp.nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket] { onReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, socket,
                &QObject::deleteLater);
    }
}

void SyncServer::onReadyRead(QTcpSocket* socket)
{
    // Accumule la requête dans une propriété du socket jusqu'à disposer
    // des en-têtes + du corps complet (Content-Length).
    QByteArray buffer =
        socket->property("nurseraBuffer").toByteArray() + socket->readAll();
    if (buffer.size() > kMaxRequestBytes) {
        socket->disconnectFromHost();
        return;
    }
    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        socket->setProperty("nurseraBuffer", buffer);
        return;
    }

    const QList<QByteArray> headerLines =
        buffer.left(headerEnd).split('\r');
    const QList<QByteArray> requestLine =
        headerLines.first().simplified().split(' ');
    if (requestLine.size() < 2) {
        socket->disconnectFromHost();
        return;
    }
    const QString method = QString::fromLatin1(requestLine.at(0)).toUpper();
    const QString path = QString::fromLatin1(requestLine.at(1))
                             .section(QLatin1Char('?'), 0, 0);

    QHash<QString, QString> headers;
    for (int i = 1; i < headerLines.size(); ++i) {
        const QByteArray line = headerLines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon > 0)
            headers.insert(
                QString::fromLatin1(line.left(colon)).toLower(),
                QString::fromLatin1(line.mid(colon + 1)).trimmed());
    }

    const qint64 contentLength =
        headers.value(QStringLiteral("content-length")).toLongLong();
    const QByteArray body = buffer.mid(headerEnd + 4);
    if (body.size() < contentLength) {
        socket->setProperty("nurseraBuffer", buffer);
        return; // corps incomplet — attendre la suite
    }

    const Reply reply =
        dispatch(method, path, headers, body.left(contentLength));

    const bool binary = !reply.raw.isEmpty();
    const QByteArray payload = binary
        ? reply.raw
        : QJsonDocument(reply.body).toJson(QJsonDocument::Compact);
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(reply.status) + ' '
        + statusText(reply.status).toLatin1() + "\r\n";
    response += "Content-Type: "
        + (binary ? reply.contentType : QByteArrayLiteral("application/json"))
        + "\r\n";
    response += "Content-Length: " + QByteArray::number(payload.size())
        + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += payload;
    socket->write(response);
    socket->disconnectFromHost();
}

QString SyncServer::bearerToken(const QHash<QString, QString>& headers) const
{
    const QString auth = headers.value(QStringLiteral("authorization"));
    if (!auth.startsWith(QStringLiteral("Bearer "), Qt::CaseInsensitive))
        return {};
    const QString token = auth.mid(7).trimmed();
    const auto it = m_sessions.constFind(token);
    if (it == m_sessions.constEnd()
        || it->expires < QDateTime::currentDateTime())
        return {};
    return token;
}

SyncServer::Reply SyncServer::dispatch(const QString& method,
                                       const QString& path,
                                       const QHash<QString, QString>& headers,
                                       const QByteArray& body)
{
    if (method == QLatin1String("GET")
        && path == QLatin1String("/api/v1/ping"))
        return handlePing();
    if (method == QLatin1String("POST")
        && path == QLatin1String("/api/v1/auth"))
        return handleAuth(body);

    // Toutes les autres routes exigent un jeton valide.
    const QString token = bearerToken(headers);
    if (token.isEmpty())
        return Reply{401,
                     QJsonObject{{QStringLiteral("error"),
                                  QStringLiteral("auth.token")},
                                 {QStringLiteral("message"),
                                  QStringLiteral("Jeton absent ou expiré.")}}};
    const int userId = m_sessions.value(token).userId;

    if (method == QLatin1String("GET")
        && path == QLatin1String("/api/v1/catalog"))
        return handleCatalog();
    if (method == QLatin1String("GET")
        && path == QLatin1String("/api/v1/stock"))
        return handleStock();
    if (method == QLatin1String("POST")
        && path == QLatin1String("/api/v1/moves"))
        return handleMoves(body, userId);
    if (method == QLatin1String("GET")
        && path.startsWith(QLatin1String("/api/v1/photo/")))
        return handlePhoto(path.section(QLatin1Char('/'), -1).toInt());

    return Reply{404,
                 QJsonObject{{QStringLiteral("error"),
                              QStringLiteral("notFound")}}};
}

SyncServer::Reply SyncServer::handlePing()
{
    int schema = -1;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        schema = query.value(0).toInt();
    return Reply{200, QJsonObject{
        {QStringLiteral("app"), QStringLiteral("nursera")},
        {QStringLiteral("schemaVersion"), schema},
    }};
}

SyncServer::Reply SyncServer::handleAuth(const QByteArray& body)
{
    const QJsonObject request = QJsonDocument::fromJson(body).object();
    const QString username =
        request.value(QStringLiteral("username")).toString().trimmed();
    const QString pin = request.value(QStringLiteral("pin")).toString();

    const auto users = m_users.activeUsers();
    if (users) {
        for (const User& user : users.value()) {
            if (user.username.compare(username, Qt::CaseInsensitive) != 0)
                continue;
            const auto hash = m_users.pinHashOf(user.id);
            if (!hash || !PasswordHasher::verify(pin, hash.value()))
                break;
            Session session;
            session.userId = user.id;
            session.role = roleCode(user.role);
            session.expires =
                QDateTime::currentDateTime().addSecs(kTokenHours * 3600);
            const QString token =
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                + QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_sessions.insert(token, session);
            return Reply{200, QJsonObject{
                {QStringLiteral("token"), token},
                {QStringLiteral("userId"), user.id},
                {QStringLiteral("name"), user.displayName},
                {QStringLiteral("role"), session.role},
            }};
        }
    }
    return Reply{401, QJsonObject{
        {QStringLiteral("error"), QStringLiteral("auth.invalid")},
        {QStringLiteral("message"),
         QStringLiteral("Utilisateur ou PIN incorrect.")},
    }};
}

SyncServer::Reply SyncServer::handleCatalog()
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);

    QJsonArray products;
    QSqlQuery productQuery(database);
    productQuery.exec(QStringLiteral(
        "SELECT p.id, p.name_fr, COALESCE(p.name_ar, ''), p.type, "
        "COALESCE(p.category_id, 0), "
        "EXISTS(SELECT 1 FROM product_photos ph "
        "       WHERE ph.product_id = p.id AND ph.is_main = 1) "
        "FROM products p WHERE p.active = 1"));
    while (productQuery.next())
        products.append(QJsonObject{
            {QStringLiteral("id"), productQuery.value(0).toInt()},
            {QStringLiteral("nameFr"), productQuery.value(1).toString()},
            {QStringLiteral("nameAr"), productQuery.value(2).toString()},
            {QStringLiteral("type"), productQuery.value(3).toString()},
            {QStringLiteral("categoryId"), productQuery.value(4).toInt()},
            {QStringLiteral("photo"), productQuery.value(5).toBool()},
        });

    QJsonArray variants;
    QSqlQuery variantQuery(database);
    variantQuery.exec(QStringLiteral(
        "SELECT v.id, v.product_id, v.packaging, v.sku, "
        "COALESCE(v.barcode, ''), v.price_ttc "
        "FROM variants v JOIN products p ON p.id = v.product_id "
        "WHERE v.active = 1 AND p.active = 1"));
    while (variantQuery.next())
        variants.append(QJsonObject{
            {QStringLiteral("id"), variantQuery.value(0).toInt()},
            {QStringLiteral("productId"), variantQuery.value(1).toInt()},
            {QStringLiteral("packaging"), variantQuery.value(2).toString()},
            {QStringLiteral("sku"), variantQuery.value(3).toString()},
            {QStringLiteral("barcode"), variantQuery.value(4).toString()},
            {QStringLiteral("priceTtc"), variantQuery.value(5).toDouble()},
        });

    QJsonArray locations;
    QSqlQuery locationQuery(database);
    locationQuery.exec(QStringLiteral(
        "SELECT id, name_fr, COALESCE(name_ar, ''), kind "
        "FROM locations WHERE active = 1"));
    while (locationQuery.next())
        locations.append(QJsonObject{
            {QStringLiteral("id"), locationQuery.value(0).toInt()},
            {QStringLiteral("nameFr"), locationQuery.value(1).toString()},
            {QStringLiteral("nameAr"), locationQuery.value(2).toString()},
            {QStringLiteral("kind"), locationQuery.value(3).toString()},
        });

    return Reply{200, QJsonObject{
        {QStringLiteral("products"), products},
        {QStringLiteral("variants"), variants},
        {QStringLiteral("locations"), locations},
    }};
}

SyncServer::Reply SyncServer::handleStock()
{
    QJsonArray levels;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.exec(QStringLiteral(
        "SELECT variant_id, location_id, qty FROM stock"));
    while (query.next())
        levels.append(QJsonObject{
            {QStringLiteral("variantId"), query.value(0).toInt()},
            {QStringLiteral("locationId"), query.value(1).toInt()},
            {QStringLiteral("qty"), query.value(2).toInt()},
        });
    return Reply{200, QJsonObject{{QStringLiteral("levels"), levels}}};
}

SyncServer::Reply SyncServer::handlePhoto(int productId)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT file_path FROM product_photos "
        "WHERE product_id = :id AND is_main = 1"));
    query.bindValue(QStringLiteral(":id"), productId);
    if (!query.exec() || !query.next())
        return Reply{404, QJsonObject{{QStringLiteral("error"),
                                       QStringLiteral("photo.notFound")}}};
    QFile file(query.value(0).toString());
    if (!file.open(QIODevice::ReadOnly))
        return Reply{404, QJsonObject{{QStringLiteral("error"),
                                       QStringLiteral("photo.unreadable")}}};
    Reply reply;
    reply.raw = file.readAll();
    reply.contentType = QByteArrayLiteral("image/jpeg");
    return reply;
}

SyncServer::Reply SyncServer::handleMoves(const QByteArray& body, int userId)
{
    const QJsonArray operations = QJsonDocument::fromJson(body)
                                      .object()
                                      .value(QStringLiteral("moves"))
                                      .toArray();
    QJsonArray results;
    for (const QJsonValue& operationValue : operations) {
        const QJsonObject operation = operationValue.toObject();
        StockMove move;
        move.uuid = operation.value(QStringLiteral("uuid")).toString();
        const QString kind = operation.value(QStringLiteral("kind")).toString();
        move.kind = kind == QLatin1String("out") ? MoveKind::Out
            : kind == QLatin1String("transfer") ? MoveKind::Transfer
            : kind == QLatin1String("adjust") ? MoveKind::Adjust
            : MoveKind::In;
        move.variantId = operation.value(QStringLiteral("variantId")).toInt();
        move.fromLocationId = operation.value(QStringLiteral("fromId")).toInt();
        move.toLocationId = operation.value(QStringLiteral("toId")).toInt();
        move.qty = operation.value(QStringLiteral("qty")).toInt();
        move.note = operation.value(QStringLiteral("note")).toString();
        move.lossReason =
            operation.value(QStringLiteral("lossReason")).toString();
        move.userId = userId;

        QJsonObject ack{{QStringLiteral("uuid"), move.uuid}};
        if (move.uuid.isEmpty()) {
            ack.insert(QStringLiteral("status"), QStringLiteral("error"));
            ack.insert(QStringLiteral("message"),
                       QStringLiteral("uuid obligatoire (idempotence)."));
        } else if (const auto recorded = m_stock.recordMove(move); recorded) {
            ack.insert(QStringLiteral("status"), QStringLiteral("ok"));
        } else {
            ack.insert(QStringLiteral("status"), QStringLiteral("error"));
            ack.insert(QStringLiteral("message"), recorded.error().message);
        }
        results.append(ack);
    }
    return Reply{200, QJsonObject{{QStringLiteral("results"), results}}};
}

} // namespace nursera
