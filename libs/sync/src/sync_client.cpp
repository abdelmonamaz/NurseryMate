#include "sync_client.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace nursera {
namespace {

QNetworkRequest jsonRequest(const QUrl& url, const QString& token = {})
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(8000); // LAN : échec rapide, pas d'attente
    if (!token.isEmpty())
        request.setRawHeader(QByteArrayLiteral("Authorization"),
                             QByteArrayLiteral("Bearer ") + token.toUtf8());
    return request;
}

QJsonObject bodyOf(QNetworkReply* reply)
{
    return QJsonDocument::fromJson(reply->readAll()).object();
}

} // namespace

SyncClient::SyncClient(MobileStore& store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
}

QString SyncClient::serverDisplay() const
{
    const QString host = m_store.metaValue(QStringLiteral("host"));
    return host.isEmpty()
        ? QString()
        : host + QLatin1Char(':')
            + m_store.metaValue(QStringLiteral("port"),
                                QStringLiteral("8477"));
}

QUrl SyncClient::apiUrl(const QString& path) const
{
    return QUrl(QStringLiteral("http://%1:%2%3")
                    .arg(m_store.metaValue(QStringLiteral("host")),
                         m_store.metaValue(QStringLiteral("port"),
                                           QStringLiteral("8477")),
                         path));
}

void SyncClient::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit stateChanged();
}

void SyncClient::login(const QString& host, const QString& port,
                       const QString& username, const QString& pin)
{
    m_store.setMetaValue(QStringLiteral("host"), host.trimmed());
    m_store.setMetaValue(QStringLiteral("port"),
                         port.trimmed().isEmpty() ? QStringLiteral("8477")
                                                  : port.trimmed());
    setBusy(true);

    QNetworkReply* reply = m_network.post(
        jsonRequest(apiUrl(QStringLiteral("/api/v1/auth"))),
        QJsonDocument(QJsonObject{
                          {QStringLiteral("username"), username.trimmed()},
                          {QStringLiteral("pin"), pin},
                      })
            .toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        setBusy(false);
        const QJsonObject body = bodyOf(reply);
        if (reply->error() != QNetworkReply::NoError
            || body.value(QStringLiteral("token")).toString().isEmpty()) {
            const QString message =
                body.value(QStringLiteral("message")).toString();
            emit loginFinished(
                false,
                message.isEmpty()
                    ? tr("Poste principal injoignable — vérifiez l'IP et "
                         "le Wi-Fi.")
                    : message);
            return;
        }
        m_token = body.value(QStringLiteral("token")).toString();
        m_store.setMetaValue(QStringLiteral("userName"),
                             body.value(QStringLiteral("name")).toString());
        emit stateChanged();
        emit loginFinished(true, QString());
        syncNow(); // première réplication dans la foulée
    });
}

void SyncClient::logout()
{
    m_token.clear();
    emit stateChanged();
}

void SyncClient::discover(const QString& port)
{
    const quint16 targetPort =
        port.trimmed().isEmpty() ? quint16(8477) : port.trimmed().toUShort();
    auto* socket = new QUdpSocket(this);
    socket->bind(); // port éphémère pour recevoir la réponse

    connect(socket, &QUdpSocket::readyRead, this, [this, socket] {
        while (socket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(int(socket->pendingDatagramSize()));
            QHostAddress sender;
            socket->readDatagram(datagram.data(), datagram.size(), &sender);
            const QJsonObject reply =
                QJsonDocument::fromJson(datagram).object();
            if (reply.value(QStringLiteral("app")).toString()
                != QLatin1String("nursera"))
                continue;
            // Normalise ::ffff:a.b.c.d -> a.b.c.d
            bool isIpv4 = false;
            const QHostAddress ipv4(sender.toIPv4Address(&isIpv4));
            const QString host =
                isIpv4 ? ipv4.toString() : sender.toString();
            socket->deleteLater();
            emit discoverFinished(
                true, host,
                QString::number(reply.value(QStringLiteral("port"))
                                    .toInt()));
            return;
        }
    });

    // Broadcast général + boucle locale (utile en préversion desktop)
    const QByteArray probe = QByteArrayLiteral("nursera-discover");
    socket->writeDatagram(probe, QHostAddress::Broadcast, targetPort);
    socket->writeDatagram(probe, QHostAddress(QStringLiteral("127.0.0.1")),
                          targetPort);

    QTimer::singleShot(1500, socket, [this, socket] {
        socket->deleteLater();
        emit discoverFinished(false, QString(), QString());
    });
}

void SyncClient::syncNow()
{
    if (m_token.isEmpty()) {
        emit syncFinished(false, tr("Connectez-vous d'abord."));
        return;
    }
    if (m_busy)
        return;
    setBusy(true);
    pushPendingThenPull();
}

void SyncClient::pushPendingThenPull()
{
    const QVariantList pending = m_store.pendingMoves();
    if (pending.isEmpty()) {
        pullCatalogThenStock();
        return;
    }

    QJsonArray moves;
    for (const QVariant& moveVariant : pending)
        moves.append(QJsonObject::fromVariantMap(moveVariant.toMap()));

    QNetworkReply* reply = m_network.post(
        jsonRequest(apiUrl(QStringLiteral("/api/v1/moves")), m_token),
        QJsonDocument(QJsonObject{{QStringLiteral("moves"), moves}})
            .toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false);
            emit syncFinished(false,
                              tr("Envoi impossible — saisies conservées, "
                                 "elles partiront à la prochaine sync."));
            return;
        }
        // Acquittement op par op : on ne retire de la file que l'accepté ;
        // une erreur métier (ex. variante supprimée) est signalée mais ne
        // bloque pas le reste.
        QStringList refused;
        const QJsonArray results =
            bodyOf(reply).value(QStringLiteral("results")).toArray();
        for (const QJsonValue& resultValue : results) {
            const QJsonObject ack = resultValue.toObject();
            if (ack.value(QStringLiteral("status")).toString()
                == QLatin1String("ok"))
                m_store.removePending(
                    ack.value(QStringLiteral("uuid")).toString());
            else
                refused.append(
                    ack.value(QStringLiteral("message")).toString());
        }
        if (!refused.isEmpty())
            emit errorOccurred(tr("%n saisie(s) refusée(s) : %1", "",
                                  refused.size())
                                   .arg(refused.first()));
        emit stateChanged();
        pullCatalogThenStock();
    });
}

void SyncClient::pullCatalogThenStock()
{
    QNetworkReply* catalogReply = m_network.get(
        jsonRequest(apiUrl(QStringLiteral("/api/v1/catalog")), m_token));
    connect(catalogReply, &QNetworkReply::finished, this,
            [this, catalogReply] {
        catalogReply->deleteLater();
        if (catalogReply->error() != QNetworkReply::NoError) {
            setBusy(false);
            emit syncFinished(false, tr("Réplication du catalogue "
                                        "impossible."));
            return;
        }
        const QJsonObject catalog = bodyOf(catalogReply);
        m_store.replaceCatalog(
            catalog.value(QStringLiteral("products")).toArray(),
            catalog.value(QStringLiteral("variants")).toArray(),
            catalog.value(QStringLiteral("locations")).toArray());

        QNetworkReply* stockReply = m_network.get(
            jsonRequest(apiUrl(QStringLiteral("/api/v1/stock")), m_token));
        connect(stockReply, &QNetworkReply::finished, this,
                [this, stockReply] {
            stockReply->deleteLater();
            if (stockReply->error() != QNetworkReply::NoError) {
                setBusy(false);
                emit syncFinished(false, tr("Réplication du stock "
                                            "impossible."));
                return;
            }
            m_store.replaceStock(
                bodyOf(stockReply).value(QStringLiteral("levels")).toArray());
            // Photos manquantes en dernier — best effort : un échec ne
            // fait pas échouer la sync (elles reviendront la prochaine).
            downloadPhotos(m_store.productsMissingPhoto());
        });
    });
}

void SyncClient::downloadPhotos(QList<int> remaining)
{
    if (remaining.isEmpty()) {
        setBusy(false);
        m_store.setMetaValue(
            QStringLiteral("lastSync"),
            QDateTime::currentDateTime().toString(
                QStringLiteral("yyyy-MM-dd HH:mm")));
        emit stateChanged();
        emit syncFinished(true, QString());
        return;
    }
    const int productId = remaining.takeFirst();
    QNetworkReply* reply = m_network.get(jsonRequest(
        apiUrl(QStringLiteral("/api/v1/photo/%1").arg(productId)), m_token));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, productId, remaining] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QDir().mkpath(m_store.photosDir());
            QFile file(m_store.photosDir()
                       + QStringLiteral("/%1.jpg").arg(productId));
            if (file.open(QIODevice::WriteOnly))
                file.write(reply->readAll());
        }
        downloadPhotos(remaining);
    });
}

QVariantList SyncClient::searchVariants(const QString& term) const
{
    return m_store.searchVariants(term);
}

QVariantList SyncClient::locationOptions() const
{
    return m_store.locationOptions();
}

QVariantList SyncClient::inventoryLines(int locationId) const
{
    return m_store.inventoryLines(locationId);
}

bool SyncClient::queueMove(const QVariantMap& data)
{
    const auto queued = m_store.queueMove(
        data.value(QStringLiteral("kind")).toString(),
        data.value(QStringLiteral("variantId")).toInt(),
        data.value(QStringLiteral("fromId")).toInt(),
        data.value(QStringLiteral("toId")).toInt(),
        data.value(QStringLiteral("qty")).toInt(),
        data.value(QStringLiteral("note")).toString(),
        data.value(QStringLiteral("lossReason")).toString());
    if (!queued) {
        emit errorOccurred(queued.error().message);
        return false;
    }
    emit stateChanged();
    emit moveQueued();
    // Tentative d'envoi opportuniste — sans réseau, la file attendra.
    if (connected() && !m_busy)
        syncNow();
    return true;
}

} // namespace nursera
