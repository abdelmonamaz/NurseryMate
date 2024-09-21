#pragma once

#include "repositories/ibatch_repository.h"
#include "repositories/ilocation_repository.h"
#include "repositories/iproduct_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de l'écran Production (M08) : lots, pertes, passage vendable.
class BatchController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList batches READ batches NOTIFY refreshed)

public:
    BatchController(IBatchRepository& batches,
                    IProductRepository& products,
                    ILocationRepository& locations,
                    QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    QVariantList batches() const { return m_batches; }

    Q_INVOKABLE void refresh();

    // Options pour les ComboBox.
    Q_INVOKABLE QVariantList productOptions() const;   // plantes uniquement
    Q_INVOKABLE QVariantList locationOptions() const;
    Q_INVOKABLE QVariantList variantOptions(int productId) const;

    // data : productId*, origin, qtyInitial*, locationId, notes.
    Q_INVOKABLE bool createBatch(const QVariantMap& data);

    // Perte typée (F08-02).
    Q_INVOKABLE bool recordLoss(int batchId, int qty, const QString& reason,
                                const QString& note);
    // Passage en vendable -> stock (RG-08.a).
    Q_INVOKABLE bool recordSellable(int batchId, int qty, int variantId,
                                    int locationId);

    // Journal des événements du lot (détail, corrections).
    Q_INVOKABLE QVariantList batchEvents(int batchId) const;
    // Traitements & interventions (F08-03).
    Q_INVOKABLE bool recordTreatment(int batchId, const QString& kind,
                                     const QString& productUsed,
                                     const QString& dose, const QString& note);
    Q_INVOKABLE QVariantList batchTreatments(int batchId) const;
    // Contre-passe le dernier événement (correction de saisie).
    Q_INVOKABLE bool cancelLastEvent(int batchId);
    // Suppression : lot jamais utilisé uniquement (norme).
    Q_INVOKABLE bool batchDeletable(int batchId) const;
    Q_INVOKABLE bool deleteBatch(int batchId);

signals:
    void refreshed();
    void batchCreated(int batchId);
    void eventRecorded();
    void errorOccurred(const QString& message);

private:
    IBatchRepository& m_repo;
    IProductRepository& m_products;
    ILocationRepository& m_locations;
    QVariantList m_batches;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
