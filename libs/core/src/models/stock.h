#pragma once

#include <QString>

namespace nursera {

// Mouvement de stock : fait additif immuable (RG-03.a, doc 04 §3.3).
// qty toujours strictement positive ; le sens vient de kind :
//   In       -> toLocationId seul
//   Out      -> fromLocationId seul
//   Transfer -> les deux (différents)
//   Adjust   -> exactement un des deux (to = écart positif, from = écart négatif)
enum class MoveKind { In, Out, Transfer, Adjust };

struct StockMove
{
    int id = 0;
    QString uuid;            // généré si vide ; idempotence de la sync (doc 02 §5.1)
    int variantId = 0;
    MoveKind kind = MoveKind::In;
    int fromLocationId = 0;  // 0 = aucun
    int toLocationId = 0;    // 0 = aucun
    int qty = 0;
    QString reason;
    QString lossReason;      // motif de perte typé (F03-08)
    QString refKind;         // sale | purchase | inventory | batch
    int refId = 0;
    QString note;
    int userId = 0;
    int deviceId = 0;
};

// Quantité d'une variante à un emplacement donné.
struct StockLevel
{
    int locationId = 0;
    QString locationFr;
    QString locationAr;
    int qty = 0;
};

// Ligne de la vue d'ensemble du stock (par variante).
struct StockOverviewRow
{
    int variantId = 0;
    QString productFr;
    QString productAr;
    QString packaging;
    QString sku;
    int qty = 0;
    int alertThreshold = -1; // -1 = pas de seuil
};

// Ligne de l'historique des mouvements (F03-07).
struct StockMoveRow
{
    int id = 0;
    QString createdAt;
    MoveKind kind = MoveKind::In;
    QString productFr;
    QString packaging;
    QString fromFr;
    QString toFr;
    int qty = 0;
    QString refKind; // sale | sale_cancel | inventory | …
    QString reason;
    QString userName;
    // Pour le contre-mouvement guidé (correction d'une saisie) :
    int variantId = 0;
    int fromLocationId = 0;
    int toLocationId = 0;
    // Motif de perte typé d'une sortie (F03-08), vide sinon.
    QString lossReason;
};

// Résultat de la recherche de variante (sélecteurs, caisse).
struct VariantPick
{
    int variantId = 0;
    QString label;          // "Romarin — godet"
    QString sku;
    qint64 priceTtcMillimes = 0;
    qint64 priceProMillimes = 0; // 0 = pas de tarif pro
    int vatRatePercent = 0;
};

} // namespace nursera
