#pragma once

#include <QString>

namespace nursera {

// Inventaire guidé par emplacement (F03-06).
// Un inventaire validé est verrouillé ; les écarts deviennent des
// mouvements d'ajustement (RG-03.c).
enum class InventoryStatus { Draft, Validated, Cancelled };

struct Inventory
{
    int id = 0;
    QString uuid;
    int locationId = 0;
    QString locationFr;
    QString locationAr;
    InventoryStatus status = InventoryStatus::Draft;
    QString startedAt;
    QString validatedAt;
};

// Ligne d'inventaire : théorique figé au démarrage, compté saisi au fil
// de l'eau. qtyCounted = -1 tant que la ligne n'a pas été comptée.
struct InventoryLine
{
    int variantId = 0;
    QString productFr;
    QString productAr;
    QString packaging;
    QString sku;
    int qtyExpected = 0;
    int qtyCounted = -1;

    bool isCounted() const { return qtyCounted >= 0; }
    int gap() const { return isCounted() ? qtyCounted - qtyExpected : 0; }
};

} // namespace nursera
