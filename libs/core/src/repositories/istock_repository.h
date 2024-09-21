#pragma once

#include "common/result.h"
#include "models/stock.h"

#include <QList>
#include <QString>

namespace nursera {

class IStockRepository
{
public:
    virtual ~IStockRepository() = default;

    // Enregistre un mouvement et met à jour la projection `stock` dans la
    // même transaction. Valide la cohérence kind/emplacements (doc 04 §3.3).
    // Idempotent par uuid : un mouvement déjà appliqué est ignoré sans erreur
    // (rejeu de sync, doc 02 §5.1). Le stock peut devenir négatif (RG-03.b).
    // ownTransaction=false : s'exécute dans la transaction déjà ouverte par
    // l'appelant (composition — ex. vente = n° + lignes + stock, tout ou rien).
    virtual Result<void> recordMove(StockMove move, bool ownTransaction = true) = 0;

    // Quantités d'une variante par emplacement.
    virtual Result<QList<StockLevel>> levelsOf(int variantId) = 0;

    // Vue d'ensemble par variante. locationId <= 0 : tous les emplacements.
    virtual Result<QList<StockOverviewRow>> overview(const QString& term,
                                                     int locationId = 0) = 0;

    // Recherche de variante pour les sélecteurs (nom FR/AR, SKU).
    virtual Result<QList<VariantPick>> searchVariants(const QString& term,
                                                      int limit = 10) = 0;

    // Historique des mouvements, plus récents d'abord (F03-07).
    // variantId <= 0 : toutes les variantes.
    virtual Result<QList<StockMoveRow>> history(int variantId = 0,
                                                int limit = 100) = 0;
};

} // namespace nursera
