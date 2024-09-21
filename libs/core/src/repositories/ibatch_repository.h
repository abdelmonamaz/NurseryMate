#pragma once

#include "common/result.h"
#include "models/batch.h"

#include <QList>

namespace nursera {

class IBatchRepository
{
public:
    virtual ~IBatchRepository() = default;

    // Crée un lot ; numéro L-AAAA-NNN sans trou (doc_counters kind='batch').
    virtual Result<Batch> create(const BatchDraft& draft) = 0;

    virtual Result<QList<BatchRow>> list(bool includeClosed = false) = 0;

    // Perte typée (F08-02) : décrémente le restant, journalise le motif.
    virtual Result<void> recordLoss(int batchId, int qty,
                                    const QString& lossReason,
                                    const QString& note, int userId) = 0;

    // Passage en vendable (F08-02, RG-08.a) : décrémente le restant et
    // crée une entrée de stock commercial (variant × emplacement),
    // ref_kind='batch'. Transaction unique.
    virtual Result<void> recordSellable(int batchId, int qty, int variantId,
                                        int locationId, int userId) = 0;

    // Journal des événements du lot, plus récents d'abord.
    virtual Result<QList<BatchEventRow>> events(int batchId) = 0;

    // Traitements & interventions (F08-03) — journal par lot.
    virtual Result<void> recordTreatment(int batchId, const QString& kind,
                                         const QString& productUsed,
                                         const QString& dose,
                                         const QString& note, int userId) = 0;
    virtual Result<QList<BatchTreatmentRow>> treatments(int batchId) = 0;

    // Correction d'une saisie : contre-passe le DERNIER événement par un
    // événement inverse (qty négative) — restant restauré, sortie de stock
    // inverse si c'était un passage en vendable, lot rouvert s'il s'était
    // auto-clôturé. Refusé si le dernier événement est déjà une correction.
    virtual Result<void> cancelLastEvent(int batchId, int userId) = 0;

    // Norme référentiel : suppression UNIQUEMENT si le lot n'a jamais eu
    // d'événement — sinon corriger par contre-passation.
    virtual Result<bool> isReferenced(int batchId) = 0;
    virtual Result<void> remove(int batchId) = 0;
};

} // namespace nursera
