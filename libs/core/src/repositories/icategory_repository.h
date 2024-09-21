#pragma once

#include "common/result.h"
#include "models/category.h"

#include <QList>

namespace nursera {

class ICategoryRepository
{
public:
    virtual ~ICategoryRepository() = default;

    virtual Result<QList<Category>> all(bool includeInactive = false) = 0;
    virtual Result<int> insert(const Category& category) = 0;
    virtual Result<void> update(const Category& category) = 0;

    // Norme référentiel (F11-06) : suppression UNIQUEMENT si jamais
    // référencée (aucun produit, aucune sous-catégorie) — sinon désactiver.
    virtual Result<bool> isReferenced(int categoryId) = 0;
    virtual Result<void> remove(int categoryId) = 0;
};

} // namespace nursera
