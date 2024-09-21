#pragma once

#include "common/result.h"
#include "repositories/istock_repository.h"
#include "repositories/iuser_repository.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QUdpSocket>

namespace nursera {

// Serveur LAN du poste principal (M09, doc 02 §4) — API REST JSON pour le
// compagnon mobile, servie par un HTTP/1.1 minimal sur QTcpServer (le
// module Qt HttpServer n'est pas requis par le kit). V1 du protocole :
//   GET  /api/v1/ping     santé + version de schéma (sans authentification)
//   POST /api/v1/auth     {username, pin} -> {token, userId, role}
//   GET  /api/v1/catalog  référentiel : produits, variantes, emplacements
//   GET  /api/v1/stock    niveaux par variante × emplacement
//   POST /api/v1/moves    lot de mouvements terrain — idempotent par uuid
// Écritures additives uniquement (doc 02 §5) : un renvoi après coupure ne
// duplique jamais rien. Jetons Bearer en mémoire (expirent avec l'app).
// Découverte (F09-09, en lieu et place du mDNS) : le serveur répond aussi
// en UDP sur le même port au datagramme « nursera-discover » — le mobile
// broadcaste et trouve l'IP tout seul ; QR et saisie manuelle en repli.
class SyncServer : public QObject
{
    Q_OBJECT

public:
    SyncServer(QString connectionName, IUserRepository& users,
               IStockRepository& stock, QObject* parent = nullptr);

    // Démarre l'écoute (port 0 = choisi par l'OS) ; retourne le port réel.
    Result<quint16> start(quint16 port);
    quint16 port() const { return m_port; }

    // Adresses IPv4 locales (affichage Réglages / saisie sur le mobile).
    static QStringList localAddresses();

private:
    struct Reply
    {
        int status = 200;
        QJsonObject body;
        // Réponse binaire (photos) : prime sur body si non vide.
        QByteArray raw;
        QByteArray contentType;
    };

    void onNewConnection();
    void onReadyRead(class QTcpSocket* socket);
    Reply dispatch(const QString& method, const QString& path,
                   const QHash<QString, QString>& headers,
                   const QByteArray& body);
    QString bearerToken(const QHash<QString, QString>& headers) const;

    Reply handlePing();
    Reply handleAuth(const QByteArray& body);
    Reply handleCatalog();
    Reply handleStock();
    Reply handleMoves(const QByteArray& body, int userId);
    Reply handlePhoto(int productId);

    QString m_connectionName;
    IUserRepository& m_users;
    IStockRepository& m_stock;
    QTcpServer m_tcp;
    QUdpSocket m_discovery;
    quint16 m_port = 0;
    struct Session { int userId = 0; QString role; QDateTime expires; };
    QHash<QString, Session> m_sessions; // token -> session
};

} // namespace nursera
