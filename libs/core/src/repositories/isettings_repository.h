#pragma once

#include "common/result.h"

#include <QList>
#include <QString>

namespace nursera {

// Compteur documentaire (F11-07) : prochain numéro à émettre pour un
// type de document (ticket/invoice/credit_note/quote/po/batch).
struct DocCounter
{
    QString kind;
    int nextNumber = 1;
};

// Paramètres clé/valeur (table settings, M11).
// Clés société : company.name, company.tagline, company.address,
// company.phone, company.tax_id.
class ISettingsRepository
{
public:
    virtual ~ISettingsRepository() = default;

    // Valeur de la clé, ou repli si absente.
    virtual QString valueOr(const QString& key, const QString& fallback = {}) = 0;
    virtual Result<void> setValue(const QString& key, const QString& value) = 0;

    // Numérotations documentaires de l'année (F11-07) — lecture seule,
    // informatif : la numérotation sans trou n'est jamais modifiable.
    virtual Result<QList<DocCounter>> documentCounters(int year) = 0;
};

} // namespace nursera
