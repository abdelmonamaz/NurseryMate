#pragma once

#include "common/result.h"
#include "models/inventory.h"

#include <QList>

namespace nursera {

class IInventoryRepository
{
public:
    virtual ~IInventoryRepository() = default;

    // Démarre un inventaire pour l'emplacement, ou reprend le brouillon
    // existant (un seul brouillon par emplacement). Au démarrage, le
    // théorique est figé depuis la projection `stock`.
    virtual Result<Inventory> startOrResume(int locationId) = 0;

    // Reprend le premier brouillon existant, quel que soit l'emplacement
    // (reprise après fermeture de l'app en plein comptage).
    virtual Result<Inventory> resumeAnyDraft() = 0;

    virtual Result<QList<InventoryLine>> linesOf(int inventoryId) = 0;

    // qty < 0 efface le comptage de la ligne.
    virtual Result<void> setCounted(int inventoryId, int variantId, int qty) = 0;

    // Valide : chaque ligne comptée avec écart génère un mouvement
    // d'ajustement (uuid déterministe -> re-validation idempotente,
    // même garantie que la sync). Les lignes non comptées sont ignorées
    // (inventaire partiel, F03-06). Verrouille l'inventaire (RG-03.c).
    virtual Result<int> validate(int inventoryId) = 0; // -> nb d'ajustements

    virtual Result<void> cancel(int inventoryId) = 0;
};

} // namespace nursera
