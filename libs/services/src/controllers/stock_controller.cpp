#include "controllers/stock_controller.h"

namespace nursera {
namespace {

MoveKind kindFromString(const QString& text)
{
    if (text == QLatin1String("out")) return MoveKind::Out;
    if (text == QLatin1String("transfer")) return MoveKind::Transfer;
    if (text == QLatin1String("adjust")) return MoveKind::Adjust;
    return MoveKind::In;
}

} // namespace

StockController::StockController(IStockRepository& stockRepository,
                                 ILocationRepository& locations,
                                 QObject* parent)
    : QObject(parent)
    , m_stock(stockRepository)
    , m_locations(locations)
{
}

void StockController::setSearchTerm(const QString& term)
{
    if (m_searchTerm == term)
        return;
    m_searchTerm = term;
    emit searchTermChanged();
    refresh();
}

void StockController::setLocationFilter(int locationId)
{
    if (m_locationFilter == locationId)
        return;
    m_locationFilter = locationId;
    emit locationFilterChanged();
    refresh();
}

void StockController::refresh()
{
    auto result = m_stock.overview(m_searchTerm, m_locationFilter);
    if (!result) {
        emit errorOccurred(result.error().message);
        return;
    }
    m_model.setRows(std::move(result.value()));
    emit refreshed();
}

QVariantList StockController::locationOptions() const
{
    QVariantList options;
    const auto locations = m_locations.all();
    if (!locations)
        return options;
    for (const Location& location : locations.value()) {
        options.append(QVariantMap{
            {QStringLiteral("id"), location.id},
            {QStringLiteral("label"), location.nameFr},
        });
    }
    return options;
}

QVariantList StockController::searchVariants(const QString& term) const
{
    QVariantList picks;
    const auto found = m_stock.searchVariants(term);
    if (!found)
        return picks;
    for (const VariantPick& pick : found.value()) {
        picks.append(QVariantMap{
            {QStringLiteral("variantId"), pick.variantId},
            {QStringLiteral("label"), pick.label},
            {QStringLiteral("sku"), pick.sku},
        });
    }
    return picks;
}

QVariantList StockController::history(int variantId) const
{
    QVariantList rows;
    const auto moves = m_stock.history(variantId);
    if (!moves)
        return rows;

    for (const StockMoveRow& row : moves.value()) {
        QString kindLabel;
        QString flow;
        switch (row.kind) {
        case MoveKind::In:
            kindLabel = tr("Entrée");
            flow = QStringLiteral("→ ") + row.toFr;
            break;
        case MoveKind::Out:
            kindLabel = tr("Sortie");
            flow = row.fromFr + QStringLiteral(" →");
            break;
        case MoveKind::Transfer:
            kindLabel = tr("Transfert");
            flow = row.fromFr + QStringLiteral(" → ") + row.toFr;
            break;
        case MoveKind::Adjust:
            kindLabel = tr("Ajustement");
            flow = row.toFr.isEmpty() ? row.fromFr + QStringLiteral(" −")
                                      : QStringLiteral("+ ") + row.toFr;
            break;
        }

        QString origin = row.refKind;
        if (origin == QLatin1String("sale")) origin = tr("vente");
        else if (origin == QLatin1String("sale_cancel")) origin = tr("annulation");
        else if (origin == QLatin1String("inventory")) origin = tr("inventaire");

        // Motif de perte typé (F03-08) — affiché avec le flux
        QString loss;
        if (row.lossReason == QLatin1String("mortality")) loss = tr("perte · mortalité");
        else if (row.lossReason == QLatin1String("disease")) loss = tr("perte · maladie");
        else if (row.lossReason == QLatin1String("frost")) loss = tr("perte · gel");
        else if (row.lossReason == QLatin1String("breakage")) loss = tr("perte · casse");
        else if (!row.lossReason.isEmpty()) loss = tr("perte · autre");
        if (!loss.isEmpty())
            origin = origin.isEmpty() ? loss : origin + QStringLiteral(" · ") + loss;

        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("dateTime"), row.createdAt.left(16)},
            {QStringLiteral("kind"), kindLabel},
            {QStringLiteral("kindCode"),
             row.kind == MoveKind::In ? QStringLiteral("in")
             : row.kind == MoveKind::Out ? QStringLiteral("out")
             : row.kind == MoveKind::Transfer ? QStringLiteral("transfer")
                                              : QStringLiteral("adjust")},
            {QStringLiteral("label"),
             QStringLiteral("%1 — %2").arg(row.productFr, row.packaging)},
            {QStringLiteral("flow"), flow},
            {QStringLiteral("qty"), row.qty},
            {QStringLiteral("origin"), origin},
            {QStringLiteral("userName"), row.userName},
            {QStringLiteral("reason"), row.reason},
            // Contre-mouvement guidé : seuls les mouvements saisis à la
            // main se corrigent ici — vente/avoir/inventaire/lot ont leur
            // propre chemin (annulation, avoir, re-comptage…).
            {QStringLiteral("manual"),
             row.refKind.isEmpty() && row.kind != MoveKind::Adjust},
            {QStringLiteral("variantId"), row.variantId},
            {QStringLiteral("fromId"), row.fromLocationId},
            {QStringLiteral("toId"), row.toLocationId},
        });
    }
    return rows;
}

bool StockController::recordMove(const QVariantMap& data)
{
    StockMove move;
    move.kind = kindFromString(data.value(QStringLiteral("kind")).toString());
    move.variantId = data.value(QStringLiteral("variantId")).toInt();
    move.fromLocationId = data.value(QStringLiteral("fromId")).toInt();
    move.toLocationId = data.value(QStringLiteral("toId")).toInt();
    move.qty = data.value(QStringLiteral("qty")).toInt();
    move.note = data.value(QStringLiteral("note")).toString().trimmed();
    move.lossReason = data.value(QStringLiteral("lossReason")).toString().trimmed();
    move.userId = m_userIdProvider ? m_userIdProvider() : 0; // F01-04

    if (move.variantId <= 0) {
        emit errorOccurred(tr("Choisissez d'abord un produit."));
        return false;
    }

    const auto recorded = m_stock.recordMove(move);
    if (!recorded) {
        emit errorOccurred(recorded.error().message);
        return false;
    }

    refresh();
    emit moveRecorded();
    return true;
}

} // namespace nursera
