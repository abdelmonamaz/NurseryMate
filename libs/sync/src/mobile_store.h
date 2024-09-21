#pragma once

#include "common/result.h"

#include <QJsonArray>
#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

namespace nursera {

// Base locale du compagnon mobile (offline-first, doc 02 §5) : réplique du
// référentiel (produits/variantes/emplacements/stock) + file d'attente des
// saisies terrain (pending_moves) envoyées à la prochaine sync — chaque
// saisie porte son uuid, l'idempotence serveur rend le rejeu sans danger.
class MobileStore
{
public:
    explicit MobileStore(QString databasePath,
                         QString connectionName = QStringLiteral("mobile_store"));
    ~MobileStore();

    Result<void> open();

    // Réplication (remplacement complet — le référentiel est petit).
    Result<void> replaceCatalog(const QJsonArray& products,
                                const QJsonArray& variants,
                                const QJsonArray& locations);
    Result<void> replaceStock(const QJsonArray& levels);

    // Saisie terrain : mise en file + effet local immédiat (optimiste).
    // Retourne l'uuid de l'opération.
    Result<QString> queueMove(const QString& kind, int variantId, int fromId,
                              int toId, int qty, const QString& note,
                              const QString& lossReason);
    QVariantList pendingMoves() const; // pour l'envoi (maps prêts pour l'API)
    int pendingCount() const;
    Result<void> removePending(const QString& uuid);

    // Consultation locale.
    QVariantList searchVariants(const QString& term, int limit = 30) const;
    QVariantList locationOptions() const;
    // Lignes de comptage d'un emplacement (inventaire terrain F09) :
    // variantes ayant du stock à cet emplacement, théorique local.
    QVariantList inventoryLines(int locationId) const;

    // Photos produits répliquées (fichiers locaux, à côté de la base).
    QString photosDir() const;
    QString photoPath(int productId) const; // "" si pas de fichier local
    // Produits marqués « photo » côté serveur dont le fichier manque.
    QList<int> productsMissingPhoto() const;

    // Paramètres persistés (serveur, jeton, dernière sync…).
    QString metaValue(const QString& key, const QString& fallback = {}) const;
    void setMetaValue(const QString& key, const QString& value);

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_databasePath;
    QString m_connectionName;
};

} // namespace nursera
