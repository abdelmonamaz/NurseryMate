#include "controllers/inventory_controller.h"

namespace nursera {

InventoryController::InventoryController(IInventoryRepository& inventories,
                                         ILocationRepository& locations,
                                         QObject* parent)
    : QObject(parent)
    , m_inventories(inventories)
    , m_locations(locations)
{
}

QVariantList InventoryController::locationOptions() const
{
    QVariantList options;
    const auto locations = m_locations.all();
    if (!locations)
        return options;
    for (const Location& location : locations.value()) {
        options.append(QVariantMap{
            {QStringLiteral("id"), location.id},
            {QStringLiteral("label"), location.nameFr},
        });
    }
    return options;
}

bool InventoryController::loadInventory(const Inventory& inventory)
{
    const auto lines = m_inventories.linesOf(inventory.id);
    if (!lines) {
        emit errorOccurred(lines.error().message);
        return false;
    }

    m_inventoryId = inventory.id;
    m_locationLabel = inventory.locationFr;
    m_model.setLines(lines.value());
    emit stateChanged();
    emit progressChanged();
    return true;
}

bool InventoryController::start(int locationId)
{
    const auto inventory = m_inventories.startOrResume(locationId);
    if (!inventory) {
        emit errorOccurred(inventory.error().message);
        return false;
    }
    return loadInventory(inventory.value());
}

void InventoryController::resumeAny()
{
    if (m_inventoryId > 0)
        return; // déjà en cours
    const auto inventory = m_inventories.resumeAnyDraft();
    if (!inventory)
        return; // pas de brouillon : silencieux
    loadInventory(inventory.value());
}

void InventoryController::setCounted(int variantId, int qty)
{
    if (m_inventoryId <= 0)
        return;
    const auto result = m_inventories.setCounted(m_inventoryId, variantId, qty);
    if (!result) {
        emit errorOccurred(result.error().message);
        return;
    }
    m_model.updateCounted(variantId, qty);
    emit progressChanged();
}

void InventoryController::confirmExpected(int variantId, int expected)
{
    setCounted(variantId, expected);
}

bool InventoryController::validate()
{
    if (m_inventoryId <= 0)
        return false;
    const auto result = m_inventories.validate(m_inventoryId);
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }

    const int adjustments = result.value();
    m_inventoryId = 0;
    m_locationLabel.clear();
    m_model.setLines({});
    emit stateChanged();
    emit progressChanged();
    emit validated(adjustments);
    return true;
}

void InventoryController::cancel()
{
    if (m_inventoryId <= 0)
        return;
    const auto result = m_inventories.cancel(m_inventoryId);
    if (!result) {
        emit errorOccurred(result.error().message);
        return;
    }
    m_inventoryId = 0;
    m_locationLabel.clear();
    m_model.setLines({});
    emit stateChanged();
    emit progressChanged();
}

} // namespace nursera
