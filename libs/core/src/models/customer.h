#pragma once

#include "common/money.h"

#include <QString>

namespace nursera {

// Client particulier ou professionnel (F06-01).
enum class CustomerKind { Individual, Professional };

struct Customer
{
    int id = 0;
    CustomerKind kind = CustomerKind::Individual;
    QString name;
    QString phone;
    QString phone2;
    QString email;
    QString address;
    QString taxId;
    QString lang = QStringLiteral("fr");
    qint64 creditLimitMillimes = -1; // -1 = pas de plafond (RG-06.b)
    QString notes;
    bool active = true;
};

// Ligne de la liste clients avec encours agrégé (F06-03).
struct CustomerRow
{
    int id = 0;
    CustomerKind kind = CustomerKind::Individual;
    QString name;
    QString phone;
    Money balance; // > 0 = le client doit de l'argent
    qint64 creditLimitMillimes = -1;
    bool active = true;
};

} // namespace nursera
