#pragma once

#include "common/money.h"

#include <QString>

namespace nursera {

// Montant en lettres pour les factures (F05-03, doc 02 §6).
// Français, convention tunisienne : dinars + millimes.
// Ex. 125,500 -> "cent vingt-cinq dinars et cinq cents millimes".
class AmountToWords
{
public:
    // Nombre entier en toutes lettres (0..999 999 999 999).
    static QString frenchNumber(qint64 n);

    // Montant TND en lettres, première lettre en majuscule.
    static QString frenchAmount(Money amount);
};

} // namespace nursera
