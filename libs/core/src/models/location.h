#pragma once

#include <QString>

namespace nursera {

// Emplacement physique (F03-01) : serre, parcelle, zone de vente, dépôt.
enum class LocationKind { Greenhouse, Field, SalesArea, Warehouse };

struct Location
{
    int id = 0;
    QString nameFr;
    QString nameAr;
    LocationKind kind = LocationKind::Greenhouse;
    bool active = true;
};

} // namespace nursera
