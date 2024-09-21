#pragma once

#include "mobile_store.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUdpSocket>
#include <QVariantList>
#include <QVariantMap>

namespace nursera {

// Client de sync du compagnon mobile (M09, doc 02 §5) : parle au serveur
// LAN du poste principal. Offline-first : toutes les saisies passent par
// la file locale (MobileStore) ; syncNow() pousse la file (idempotente par
// uuid) puis re-tire référentiel et stock. L'app n'exige JAMAIS le réseau
// pour saisir.
class SyncClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY stateChanged)
    Q_PROPERTY(QString serverDisplay READ serverDisplay NOTIFY stateChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY stateChanged)
    Q_PROPERTY(QString lastSync READ lastSync NOTIFY stateChanged)

public:
    explicit SyncClient(MobileStore& store, QObject* parent = nullptr);

    bool connected() const { return !m_token.isEmpty(); }
    bool busy() const { return m_busy; }
    int pendingCount() const { return m_store.pendingCount(); }
    QString serverDisplay() const;
    QString userName() const
    {
        return m_store.metaValue(QStringLiteral("userName"));
    }
    QString lastSync() const
    {
        return m_store.metaValue(QStringLiteral("lastSync"));
    }

    // Connexion au poste principal : host/port mémorisés, PIN jamais stocké.
    Q_INVOKABLE void login(const QString& host, const QString& port,
                           const QString& username, const QString& pin);
    Q_INVOKABLE void logout();

    // Découverte du poste principal par broadcast UDP (F09-09) —
    // émet discoverFinished(found, host, port) sous ~1,5 s.
    Q_INVOKABLE void discover(const QString& port);

    // Push de la file puis pull référentiel + stock.
    Q_INVOKABLE void syncNow();

    // Consultation et saisie locales (déléguées au MobileStore).
    Q_INVOKABLE QVariantList searchVariants(const QString& term) const;
    Q_INVOKABLE QVariantList locationOptions() const;
    Q_INVOKABLE QVariantList inventoryLines(int locationId) const;
    // data : kind (in|out|transfer), variantId*, fromId, toId, qty*,
    //        note, lossReason. Ne requiert PAS le réseau.
    Q_INVOKABLE bool queueMove(const QVariantMap& data);

    // Hôte/port mémorisés (pré-remplissage de l'écran de connexion).
    Q_INVOKABLE QString savedHost() const
    {
        return m_store.metaValue(QStringLiteral("host"));
    }
    Q_INVOKABLE QString savedPort() const
    {
        return m_store.metaValue(QStringLiteral("port"),
                                 QStringLiteral("8477"));
    }

signals:
    void stateChanged();
    void loginFinished(bool ok, const QString& message);
    void syncFinished(bool ok, const QString& message);
    void discoverFinished(bool found, const QString& host,
                          const QString& port);
    void moveQueued();
    void errorOccurred(const QString& message);

private:
    QUrl apiUrl(const QString& path) const;
    void setBusy(bool busy);
    void pushPendingThenPull();
    void pullCatalogThenStock();
    void downloadPhotos(QList<int> remaining);

    MobileStore& m_store;
    QNetworkAccessManager m_network;
    QString m_token;
    bool m_busy = false;
};

} // namespace nursera
