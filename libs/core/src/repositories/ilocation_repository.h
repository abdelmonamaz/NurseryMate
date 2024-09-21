#pragma once

#include "common/result.h"
#include "models/location.h"

#include <QList>

namespace nursera {

class ILocationRepository
{
public:
    virtual ~ILocationRepository() = default;

    virtual Result<QList<Location>> all(bool includeInactive = false) = 0;
    virtual Result<int> insert(const Location& location) = 0;
    virtual Result<void> update(const Location& location) = 0;

    // Norme référentiel (F11-06) : suppression UNIQUEMENT si jamais
    // référencé (aucun mouvement, inventaire ou lot) — sinon désactiver.
    virtual Result<bool> isReferenced(int locationId) = 0;
    virtual Result<void> remove(int locationId) = 0;
};

} // namespace nursera
