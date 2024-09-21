#pragma once

#include <QString>

namespace nursera {

// Origine d'un lot de production (F08-01).
enum class BatchOrigin { Seed, Cutting, Division, YoungPlant };
enum class BatchStatus { Growing, Closed };

// Création d'un lot.
struct BatchDraft
{
    int productId = 0;
    BatchOrigin origin = BatchOrigin::Seed;
    int qtyInitial = 0;
    int locationId = 0;
    QString notes;
    int userId = 0;
};

// Lot de production créé.
struct Batch
{
    int id = 0;
    QString uuid;
    QString number; // L-AAAA-NNN
};

// Ligne de la liste des lots avec statistiques (RG-08.b).
struct BatchRow
{
    int id = 0;
    int productId = 0;
    QString number;
    QString productFr;
    BatchOrigin origin = BatchOrigin::Seed;
    int qtyInitial = 0;
    int qtyRemaining = 0;
    int qtyLost = 0;     // somme des pertes
    int qtySellable = 0; // somme passée en vendable
    QString locationFr;
    BatchStatus status = BatchStatus::Growing;
    QString startedAt;

    // Taux de survie (F08-05) = (initial − pertes) / initial, en %.
    // NB : les contre-événements (qty négative) se neutralisent dans
    // qtyLost/qtySellable — les stats restent nettes après correction.
    int survivalPercent() const
    {
        return qtyInitial > 0
            ? (qtyInitial - qtyLost) * 100 / qtyInitial
            : 0;
    }
};

// Traitement / intervention sur un lot (F08-03) : arrosage exceptionnel,
// fertilisation, phyto (produit + dose), taille, autre.
struct BatchTreatmentRow
{
    int id = 0;
    QString kind; // watering | fertilization | phyto | pruning | other
    QString productUsed;
    QString dose;
    QString note;
    QString createdAt;
    QString userName;
};

// Événement du journal d'un lot (perte, vendable, corrections).
struct BatchEventRow
{
    int id = 0;
    QString kind; // loss | sellable
    int qty = 0;  // négatif = contre-événement (correction)
    QString lossReason;
    QString variantLabel; // « pot17 » si vendable
    QString locationFr;
    QString note;
    QString createdAt;
    QString userName;
};

} // namespace nursera
