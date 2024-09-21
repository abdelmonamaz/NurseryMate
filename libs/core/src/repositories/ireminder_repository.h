#pragma once

#include "common/result.h"

#include <QList>
#include <QString>

namespace nursera {

// Rappel d'entretien (F08-04) : « fertiliser Serre 1 dans 15 j ».
struct ReminderRow
{
    int id = 0;
    QString label;
    QString dueDate; // ISO
    QString batchNumber; // vide si rappel libre
    bool overdue = false;
};

class IReminderRepository
{
public:
    virtual ~IReminderRepository() = default;

    // Rappels non faits, échus d'abord puis par échéance croissante.
    virtual Result<QList<ReminderRow>> pending() = 0;
    virtual Result<int> add(const QString& label, const QString& dueDate,
                            int batchId, int userId) = 0;
    // Fait = terminé, il disparaît de la liste (trace conservée en base).
    virtual Result<void> markDone(int reminderId) = 0;
};

} // namespace nursera
