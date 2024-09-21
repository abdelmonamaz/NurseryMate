#include "controllers/batch_controller.h"

namespace nursera {
namespace {

BatchOrigin originFromCode(const QString& code)
{
    if (code == QLatin1String("cutting")) return BatchOrigin::Cutting;
    if (code == QLatin1String("division")) return BatchOrigin::Division;
    if (code == QLatin1String("young_plant")) return BatchOrigin::YoungPlant;
    return BatchOrigin::Seed;
}

QString originLabel(BatchOrigin origin)
{
    switch (origin) {
    case BatchOrigin::Cutting: return QObject::tr("Bouturage");
    case BatchOrigin::Division: return QObject::tr("Division");
    case BatchOrigin::YoungPlant: return QObject::tr("Jeune plant acheté");
    case BatchOrigin::Seed: break;
    }
    return QObject::tr("Semis");
}

} // namespace

BatchController::BatchController(IBatchRepository& batches,
                                 IProductRepository& products,
                                 ILocationRepository& locations,
                                 QObject* parent)
    : QObject(parent)
    , m_repo(batches)
    , m_products(products)
    , m_locations(locations)
{
}

void BatchController::refresh()
{
    const auto list = m_repo.list();
    if (!list) {
        emit errorOccurred(list.error().message);
        return;
    }
    m_batches.clear();
    for (const BatchRow& row : list.value()) {
        m_batches.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("productId"), row.productId},
            {QStringLiteral("number"), row.number},
            {QStringLiteral("productFr"), row.productFr},
            {QStringLiteral("origin"), originLabel(row.origin)},
            {QStringLiteral("qtyInitial"), row.qtyInitial},
            {QStringLiteral("qtyRemaining"), row.qtyRemaining},
            {QStringLiteral("qtyLost"), row.qtyLost},
            {QStringLiteral("qtySellable"), row.qtySellable},
            {QStringLiteral("locationFr"), row.locationFr},
            {QStringLiteral("survivalPercent"), row.survivalPercent()},
            {QStringLiteral("startedAt"), row.startedAt.left(10)},
        });
    }
    emit refreshed();
}

QVariantList BatchController::productOptions() const
{
    QVariantList options;
    // Les lots concernent les plantes produites (pas les articles).
    const auto products = m_products.search(QString());
    if (!products)
        return options;
    for (const ProductRow& row : products.value()) {
        if (row.type != ProductType::Plant)
            continue;
        options.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("label"), row.nameFr},
        });
    }
    return options;
}

QVariantList BatchController::locationOptions() const
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

QVariantList BatchController::variantOptions(int productId) const
{
    QVariantList options;
    const auto variants = m_products.variantsOf(productId);
    if (!variants)
        return options;
    for (const Variant& v : variants.value()) {
        options.append(QVariantMap{
            {QStringLiteral("id"), v.id},
            {QStringLiteral("label"), v.packaging},
        });
    }
    return options;
}

bool BatchController::createBatch(const QVariantMap& data)
{
    BatchDraft draft;
    draft.productId = data.value(QStringLiteral("productId")).toInt();
    draft.qtyInitial = data.value(QStringLiteral("qtyInitial")).toInt();
    if (draft.productId <= 0 || draft.qtyInitial <= 0) {
        emit errorOccurred(tr("Choisissez un produit et une quantité initiale."));
        return false;
    }
    draft.origin = originFromCode(data.value(QStringLiteral("origin")).toString());
    draft.locationId = data.value(QStringLiteral("locationId")).toInt();
    draft.notes = data.value(QStringLiteral("notes")).toString().trimmed();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;

    const auto created = m_repo.create(draft);
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit batchCreated(created.value().id);
    return true;
}

bool BatchController::recordLoss(int batchId, int qty, const QString& reason,
                                 const QString& note)
{
    const auto result = m_repo.recordLoss(
        batchId, qty, reason, note, m_userIdProvider ? m_userIdProvider() : 0);
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    emit eventRecorded();
    return true;
}

bool BatchController::recordSellable(int batchId, int qty, int variantId,
                                     int locationId)
{
    const auto result = m_repo.recordSellable(
        batchId, qty, variantId, locationId,
        m_userIdProvider ? m_userIdProvider() : 0);
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    emit eventRecorded();
    return true;
}

QVariantList BatchController::batchEvents(int batchId) const
{
    QVariantList rows;
    const auto events = m_repo.events(batchId);
    if (!events)
        return rows;
    for (const BatchEventRow& event : events.value()) {
        QString label;
        if (event.kind == QLatin1String("loss")) {
            label = event.qty < 0 ? tr("↩ Annulation de perte")
                                  : tr("Perte");
            const QString reason =
                event.lossReason == QLatin1String("mortality") ? tr("mortalité")
                : event.lossReason == QLatin1String("disease") ? tr("maladie")
                : event.lossReason == QLatin1String("frost") ? tr("gel")
                : event.lossReason == QLatin1String("breakage") ? tr("casse")
                : event.lossReason.isEmpty() ? QString() : tr("autre");
            if (!reason.isEmpty())
                label += QStringLiteral(" · ") + reason;
        } else {
            label = event.qty < 0 ? tr("↩ Annulation vendable")
                                  : tr("Passage en vendable");
            if (!event.variantLabel.isEmpty())
                label += QStringLiteral(" · ") + event.variantLabel;
            if (!event.locationFr.isEmpty())
                label += QStringLiteral(" → ") + event.locationFr;
        }
        rows.append(QVariantMap{
            {QStringLiteral("id"), event.id},
            {QStringLiteral("kind"), event.kind},
            {QStringLiteral("qty"), event.qty},
            {QStringLiteral("isCorrection"), event.qty < 0},
            {QStringLiteral("label"), label},
            {QStringLiteral("note"), event.note},
            {QStringLiteral("dateTime"), event.createdAt.left(16)},
            {QStringLiteral("userName"), event.userName},
        });
    }
    return rows;
}

bool BatchController::recordTreatment(int batchId, const QString& kind,
                                      const QString& productUsed,
                                      const QString& dose, const QString& note)
{
    const auto result = m_repo.recordTreatment(
        batchId, kind, productUsed, dose, note,
        m_userIdProvider ? m_userIdProvider() : 0);
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    emit eventRecorded();
    return true;
}

QVariantList BatchController::batchTreatments(int batchId) const
{
    QVariantList rows;
    const auto treatments = m_repo.treatments(batchId);
    if (!treatments)
        return rows;
    for (const BatchTreatmentRow& treatment : treatments.value()) {
        const QString kindLabel =
            treatment.kind == QLatin1String("watering") ? tr("Arrosage")
            : treatment.kind == QLatin1String("fertilization") ? tr("Fertilisation")
            : treatment.kind == QLatin1String("phyto") ? tr("Traitement phyto")
            : treatment.kind == QLatin1String("pruning") ? tr("Taille")
            : tr("Autre intervention");
        QString detail = kindLabel;
        if (!treatment.productUsed.isEmpty())
            detail += QStringLiteral(" · ") + treatment.productUsed;
        if (!treatment.dose.isEmpty())
            detail += QStringLiteral(" · ") + treatment.dose;
        rows.append(QVariantMap{
            {QStringLiteral("id"), treatment.id},
            {QStringLiteral("kind"), treatment.kind},
            {QStringLiteral("label"), detail},
            {QStringLiteral("note"), treatment.note},
            {QStringLiteral("dateTime"), treatment.createdAt.left(16)},
            {QStringLiteral("userName"), treatment.userName},
        });
    }
    return rows;
}

bool BatchController::cancelLastEvent(int batchId)
{
    const auto result = m_repo.cancelLastEvent(
        batchId, m_userIdProvider ? m_userIdProvider() : 0);
    if (!result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    emit eventRecorded();
    return true;
}

bool BatchController::batchDeletable(int batchId) const
{
    const auto referenced = m_repo.isReferenced(batchId);
    return referenced.isOk() && !referenced.value();
}

bool BatchController::deleteBatch(int batchId)
{
    const auto removed = m_repo.remove(batchId);
    if (!removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    refresh();
    emit eventRecorded();
    return true;
}

} // namespace nursera
