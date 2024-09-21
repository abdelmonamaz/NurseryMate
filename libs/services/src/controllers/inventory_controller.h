#pragma once

#include "qml_models/inventory_list_model.h"
#include "repositories/iinventory_repository.h"
#include "repositories/ilocation_repository.h"

#include <QObject>
#include <QVariantList>

namespace nursera {

// Controller de l'inventaire guidé (F03-06).
class InventoryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(nursera::InventoryListModel* lines READ lines CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    Q_PROPERTY(QString locationLabel READ locationLabel NOTIFY stateChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY progressChanged)
    Q_PROPERTY(int countedCount READ countedCount NOTIFY progressChanged)
    Q_PROPERTY(int gapCount READ gapCount NOTIFY progressChanged)
    Q_PROPERTY(int gapTotal READ gapTotal NOTIFY progressChanged)

public:
    InventoryController(IInventoryRepository& inventories,
                        ILocationRepository& locations,
                        QObject* parent = nullptr);

    InventoryListModel* lines() { return &m_model; }
    bool active() const { return m_inventoryId > 0; }
    QString locationLabel() const { return m_locationLabel; }
    int totalCount() const { return m_model.rowCount(); }
    int countedCount() const { return m_model.countedCount(); }
    int gapCount() const { return m_model.gapCount(); }
    int gapTotal() const { return m_model.gapTotal(); }

    Q_INVOKABLE QVariantList locationOptions() const;

    // Démarre ou reprend le brouillon de l'emplacement (F03-06).
    Q_INVOKABLE bool start(int locationId);

    // Reprise silencieuse d'un comptage en cours à l'ouverture de l'écran.
    Q_INVOKABLE void resumeAny();

    Q_INVOKABLE void setCounted(int variantId, int qty);
    // 1 tap « Identique » : compté = théorique (doc 03 §4.5).
    Q_INVOKABLE void confirmExpected(int variantId, int expected);

    // Validation irréversible (RG-03.c) -> ajustements de stock.
    Q_INVOKABLE bool validate();
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void progressChanged();
    void validated(int adjustments);
    void errorOccurred(const QString& message);

private:
    bool loadInventory(const Inventory& inventory);

    IInventoryRepository& m_inventories;
    ILocationRepository& m_locations;
    InventoryListModel m_model;
    int m_inventoryId = 0;
    QString m_locationLabel;
};

} // namespace nursera
