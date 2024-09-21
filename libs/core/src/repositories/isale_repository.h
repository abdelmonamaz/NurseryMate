#pragma once

#include "common/result.h"
#include "models/sale.h"

#include <QList>

namespace nursera {

class ISaleRepository
{
public:
    virtual ~ISaleRepository() = default;

    // Enregistre la vente dans UNE transaction : numéro sans trou depuis
    // doc_counters (RG-04.b), lignes (snapshot), paiement, et sorties de
    // stock refKind='sale' (RG-04.a). Tout ou rien.
    virtual Result<Sale> record(const SaleDraft& draft) = 0;

    // Détail complet d'une vente — ticket et réimpression (F04-06).
    virtual Result<SaleDetails> details(int saleId) = 0;

    // Annulation le jour même, motif obligatoire (F04-08, Gérant) :
    // contre-mouvements de stock idempotents + statut cancelled.
    // Au-delà du jour même -> refus (retour/avoir en V1).
    virtual Result<void> cancel(int saleId, const QString& reason,
                                int userId) = 0;

    // Avoir (note de crédit) : contre-passation APRÈS le jour même — la
    // norme interdit toute suppression. UNE transaction : numéro AV-AAAA-NNN
    // sans trou, retour stock optionnel (idempotent). PARTIEL admis :
    // draft.lines choisit lignes et quantités (vide = tout le restant),
    // remise globale reprise au prorata, plusieurs avoirs par vente.
    virtual Result<CreditNote> createCreditNote(const CreditNoteDraft& draft) = 0;

    // Lignes de la vente avec leur déjà-remboursé (préparer un avoir).
    virtual Result<QList<RefundableLine>> refundableLines(int saleId) = 0;

    // Détail du DERNIER avoir de la vente (PDF) — entête + lignes
    // remboursées + vente de référence.
    virtual Result<CreditNoteDetails> creditNoteDetails(int saleId) = 0;

    // Journal du jour (F04-07), plus récentes d'abord.
    virtual Result<QList<SaleJournalRow>> todayJournal() = 0;
    virtual Result<DayTotals> todayTotals() = 0;

    // Clôture de caisse (F04-09) : enregistre le compté vs le théorique
    // espèces du jour, retourne l'écart (compté − théorique).
    virtual Result<Money> recordCashClosure(Money countedCash, int userId) = 0;
};

} // namespace nursera
