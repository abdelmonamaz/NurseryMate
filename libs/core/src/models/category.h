#pragma once

#include <QString>

namespace nursera {

// Catégorie hiérarchique à 2 niveaux (F02-02).
struct Category
{
    int id = 0;
    int parentId = 0; // 0 = catégorie racine
    QString nameFr;
    QString nameAr;
    int sortOrder = 0;
    bool active = true;
};

} // namespace nursera
