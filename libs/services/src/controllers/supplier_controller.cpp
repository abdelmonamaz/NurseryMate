#include "controllers/supplier_controller.h"

#include <QHash>
#include <QLocale>
#include <QSet>

#include <algorithm>

namespace nursera {
namespace {

QString orderStatusLabel(const QString& code)
{
    if (code == QLatin1String("draft")) return SupplierController::tr("Brouillon");
    if (code == QLatin1String("sent")) return SupplierController::tr("Envoyée");
    if (code == QLatin1String("partial")) return SupplierController::tr("Partielle");
    if (code == QLatin1String("received")) return SupplierController::tr("Reçue");
    if (code == QLatin1String("cancelled")) return SupplierController::tr("Annulée");
    return code;
}

} // namespace

SupplierController::SupplierController(ISupplierRepository& suppliers,
                                       QObject* parent)
    : QObject(parent)
    , m_repository(suppliers)
{
}

void SupplierController::setSearchTerm(const QString& term)
{
    if (m_searchTerm == term)
        return;
    m_searchTerm = term;
    emit searchTermChanged();
    refresh();
}

void SupplierController::setShowInactive(bool show)
{
    if (m_showInactive == show)
        return;
    m_showInactive = show;
    emit showInactiveChanged();
    refresh();
}

void SupplierController::refresh()
{
    const QLocale locale;

    const auto suppliers = m_repository.search(m_searchTerm, m_showInactive);
    if (!suppliers) {
        emit errorOccurred(suppliers.error().message);
        return;
    }
    m_suppliers.clear();
    for (const Supplier& supplier : suppliers.value()) {
        m_suppliers.append(QVariantMap{
            {QStringLiteral("id"), supplier.id},
            {QStringLiteral("name"), supplier.name},
            {QStringLiteral("phone"), supplier.phone},
            {QStringLiteral("email"), supplier.email},
            {QStringLiteral("address"), supplier.address},
            {QStringLiteral("taxId"), supplier.taxId},
            {QStringLiteral("paymentTerms"), supplier.paymentTerms},
            {QStringLiteral("notes"), supplier.notes},
            {QStringLiteral("supplies"), supplier.supplies},
            {QStringLiteral("active"), supplier.active},
            {QStringLiteral("hasDebt"), supplier.balance.millimes() > 0},
            {QStringLiteral("balanceDisplay"),
             supplier.balance.toDisplayString(locale)},
        });
    }

    m_receipts.clear();
    if (const auto receipts = m_repository.recentReceipts()) {
        for (const ReceiptRow& row : receipts.value()) {
            m_receipts.append(QVariantMap{
                {QStringLiteral("id"), row.id},
                {QStringLiteral("dateTime"), row.receivedAt.left(16)},
                {QStringLiteral("supplierName"),
                 row.supplierName.isEmpty() ? tr("(sans fournisseur)")
                                            : row.supplierName},
                {QStringLiteral("locationFr"), row.locationFr},
                {QStringLiteral("lineCount"), row.lineCount},
                {QStringLiteral("totalQty"), row.totalQty},
                {QStringLiteral("totalCost"), row.totalCost.toDisplayString(locale)},
            });
        }
    }

    m_orders.clear();
    if (const auto orders = m_repository.recentOrders()) {
        for (const PoRow& row : orders.value()) {
            m_orders.append(QVariantMap{
                {QStringLiteral("id"), row.id},
                {QStringLiteral("number"), row.number},
                {QStringLiteral("date"), row.createdAt.left(10)},
                {QStringLiteral("supplierName"), row.supplierName},
                {QStringLiteral("status"), row.status},
                {QStringLiteral("statusLabel"), orderStatusLabel(row.status)},
                {QStringLiteral("lineCount"), row.lineCount},
                {QStringLiteral("qtyOrdered"), row.qtyOrdered},
                {QStringLiteral("qtyReceived"), row.qtyReceived},
                {QStringLiteral("totalCost"), row.totalCost.toDisplayString(locale)},
            });
        }
    }
    emit refreshed();
}

namespace {

// Remplit la fiche depuis le QVariantMap du formulaire QML.
void fillSupplier(Supplier& supplier, const QVariantMap& data)
{
    supplier.name = data.value(QStringLiteral("name")).toString().trimmed();
    supplier.phone = data.value(QStringLiteral("phone")).toString().trimmed();
    supplier.email = data.value(QStringLiteral("email")).toString().trimmed();
    supplier.address = data.value(QStringLiteral("address")).toString().trimmed();
    supplier.taxId = data.value(QStringLiteral("taxId")).toString().trimmed();
    supplier.paymentTerms =
        data.value(QStringLiteral("paymentTerms")).toString().trimmed();
    supplier.notes = data.value(QStringLiteral("notes")).toString().trimmed();
    supplier.supplies = data.value(QStringLiteral("supplies")).toString().trimmed();
}

} // namespace

bool SupplierController::createSupplier(const QVariantMap& data)
{
    Supplier supplier;
    fillSupplier(supplier, data);
    if (supplier.name.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }

    const auto created = m_repository.insert(supplier);
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit supplierCreated(created.value());
    return true;
}

bool SupplierController::updateSupplier(const QVariantMap& data)
{
    Supplier supplier;
    supplier.id = data.value(QStringLiteral("id")).toInt();
    fillSupplier(supplier, data);
    if (supplier.id <= 0 || supplier.name.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }

    const auto updated = m_repository.update(supplier);
    if (!updated) {
        emit errorOccurred(updated.error().message);
        return false;
    }
    refresh();
    emit supplierUpdated();
    return true;
}

bool SupplierController::setSupplierActive(int supplierId, bool active)
{
    const auto changed = m_repository.setActive(supplierId, active);
    if (!changed) {
        emit errorOccurred(changed.error().message);
        return false;
    }
    refresh();
    emit supplierUpdated();
    return true;
}

bool SupplierController::recordReceipt(const QVariantMap& data)
{
    ReceiptDraft draft;
    draft.supplierId = data.value(QStringLiteral("supplierId")).toInt();
    draft.locationId = data.value(QStringLiteral("locationId")).toInt();
    draft.note = data.value(QStringLiteral("note")).toString().trimmed();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;

    const QVariantList lines = data.value(QStringLiteral("lines")).toList();
    for (const QVariant& entry : lines) {
        const QVariantMap map = entry.toMap();
        ReceiptLine line;
        line.variantId = map.value(QStringLiteral("variantId")).toInt();
        line.qty = map.value(QStringLiteral("qty")).toInt();
        const qint64 cost =
            Money::parseMillimes(map.value(QStringLiteral("cost")).toString());
        if (cost < 0) {
            emit errorOccurred(tr("Coût invalide — format attendu : 12,500"));
            return false;
        }
        line.unitCost = Money::fromMillimes(cost);
        draft.lines.append(line);
    }

    if (draft.lines.isEmpty()) {
        emit errorOccurred(tr("Ajoutez au moins une ligne."));
        return false;
    }

    const auto recorded = m_repository.recordReceipt(draft);
    if (!recorded) {
        emit errorOccurred(recorded.error().message);
        return false;
    }
    refresh();
    emit receiptRecorded();
    return true;
}

// ── Paiements fournisseurs (F07-05) ───────────────────────────

bool SupplierController::paySupplier(const QVariantMap& data)
{
    const int supplierId = data.value(QStringLiteral("supplierId")).toInt();
    const qint64 amount =
        Money::parseMillimes(data.value(QStringLiteral("amount")).toString());
    if (amount <= 0) {
        emit errorOccurred(tr("Montant invalide — format attendu : 120,000"));
        return false;
    }
    const auto paid = m_repository.recordSupplierPayment(
        supplierId, Money::fromMillimes(amount),
        data.value(QStringLiteral("method"), QStringLiteral("cash")).toString(),
        data.value(QStringLiteral("chequeNumber")).toString().trimmed(),
        data.value(QStringLiteral("chequeDue")).toString().trimmed(),
        data.value(QStringLiteral("note")).toString().trimmed(),
        m_userIdProvider ? m_userIdProvider() : 0);
    if (!paid) {
        emit errorOccurred(paid.error().message);
        return false;
    }
    refresh();
    emit supplierPaid();
    return true;
}

QVariantList SupplierController::supplierPayments(int supplierId)
{
    QVariantList list;
    const auto payments = m_repository.paymentsOf(supplierId);
    if (!payments)
        return list;
    const QLocale locale;
    for (const SupplierPaymentRow& row : payments.value()) {
        list.append(QVariantMap{
            {QStringLiteral("date"), row.date},
            {QStringLiteral("amount"), row.amount.toDisplayString(locale)},
            {QStringLiteral("method"),
             row.method == QLatin1String("cheque") ? tr("Chèque")
                 : row.method == QLatin1String("transfer") ? tr("Virement")
                                                           : tr("Espèces")},
            {QStringLiteral("chequeNumber"), row.chequeNumber},
            {QStringLiteral("chequeDue"), row.chequeDue},
            {QStringLiteral("note"), row.note},
        });
    }
    return list;
}

QString SupplierController::supplierBalanceDisplay(int supplierId)
{
    const auto balance = m_repository.supplierBalance(supplierId);
    return balance ? balance.value().toDisplayString(QLocale()) : QString();
}

QVariantList SupplierController::dueCheques()
{
    QVariantList list;
    const auto cheques = m_repository.dueCheques();
    if (!cheques)
        return list;
    const QLocale locale;
    for (const DueChequeRow& row : cheques.value()) {
        list.append(QVariantMap{
            {QStringLiteral("supplierName"), row.supplierName},
            {QStringLiteral("amount"), row.amount.toDisplayString(locale)},
            {QStringLiteral("chequeNumber"), row.chequeNumber},
            {QStringLiteral("dueDate"), row.dueDate},
            {QStringLiteral("overdue"), row.overdue},
        });
    }
    return list;
}

// ── Catalogue fournisseur (F07-06/07/09/10) ───────────────────

QVariantList SupplierController::supplierProducts(int supplierId)
{
    QVariantList list;
    const auto products = m_repository.productsOf(supplierId);
    if (!products) {
        emit errorOccurred(products.error().message);
        return list;
    }
    const QLocale locale;
    for (const SupplierProductRow& row : products.value()) {
        list.append(QVariantMap{
            {QStringLiteral("variantId"), row.variantId},
            {QStringLiteral("label"), row.label},
            {QStringLiteral("sku"), row.sku},
            {QStringLiteral("linked"), row.linked},
            {QStringLiteral("qtySupplied"), row.qtySupplied},
            {QStringLiteral("deliveryCount"), row.deliveryCount},
            {QStringLiteral("lastCost"),
             row.deliveryCount > 0 ? row.lastCost.toDisplayString(locale)
                                   : QString()},
            {QStringLiteral("avgCost"),
             row.deliveryCount > 0 ? row.avgCost.toDisplayString(locale)
                                   : QString()},
            {QStringLiteral("lastDelivery"), row.lastDelivery},
        });
    }
    return list;
}

bool SupplierController::linkProduct(int supplierId, int variantId)
{
    if (supplierId <= 0 || variantId <= 0)
        return false;
    const auto linked = m_repository.linkProduct(supplierId, variantId);
    if (!linked) {
        emit errorOccurred(linked.error().message);
        return false;
    }
    return true;
}

bool SupplierController::unlinkProduct(int supplierId, int variantId)
{
    const auto unlinked = m_repository.unlinkProduct(supplierId, variantId);
    if (!unlinked) {
        emit errorOccurred(unlinked.error().message);
        return false;
    }
    return true;
}

QVariantList SupplierController::offersFor(int variantId)
{
    QVariantList list;
    const auto offers = m_repository.suppliersFor(variantId);
    if (!offers) {
        emit errorOccurred(offers.error().message);
        return list;
    }
    const QLocale locale;
    bool first = true;
    for (const SupplierOfferRow& row : offers.value()) {
        const bool delivered = row.deliveryCount > 0;
        list.append(QVariantMap{
            {QStringLiteral("supplierId"), row.supplierId},
            {QStringLiteral("supplierName"), row.supplierName},
            {QStringLiteral("linked"), row.linked},
            {QStringLiteral("qtySupplied"), row.qtySupplied},
            {QStringLiteral("deliveryCount"), row.deliveryCount},
            {QStringLiteral("lastCost"),
             delivered ? row.lastCost.toDisplayString(locale) : QString()},
            {QStringLiteral("avgCost"),
             delivered ? row.avgCost.toDisplayString(locale) : QString()},
            {QStringLiteral("lastDelivery"), row.lastDelivery},
            // Meilleure offre = 1er livré (tri par dernier prix croissant)
            {QStringLiteral("best"), delivered && first},
        });
        if (delivered)
            first = false;
    }
    return list;
}

QVariantList SupplierController::priceHistory(int variantId)
{
    QVariantList points;
    const auto history = m_repository.priceHistoryOf(variantId);
    if (!history)
        return points;
    const QLocale locale;
    for (const PricePoint& point : history.value()) {
        points.append(QVariantMap{
            {QStringLiteral("label"), point.date.mid(5)}, // MM-JJ
            {QStringLiteral("value"),
             static_cast<double>(point.unitCost.millimes())},
            {QStringLiteral("display"), point.unitCost.toDisplayString(locale)},
            {QStringLiteral("fullLabel"),
             point.supplierName.isEmpty()
                 ? point.date
                 : QStringLiteral("%1 · %2").arg(point.date,
                                                 point.supplierName)},
        });
    }
    return points;
}

QString SupplierController::lastCost(int supplierId, int variantId)
{
    const auto offers = m_repository.suppliersFor(variantId);
    if (!offers)
        return {};
    for (const SupplierOfferRow& row : offers.value()) {
        if (row.supplierId == supplierId && row.deliveryCount > 0)
            return QString::number(row.lastCost.millimes() / 1000.0, 'f', 3)
                .replace(QLatin1Char('.'), QLatin1Char(','));
    }
    return {};
}

QVariantList SupplierController::supplierMatches(const QVariantList& lines,
                                                 const QString& sortBy)
{
    // Agrégat par fournisseur sur l'ensemble des lignes demandées.
    struct Match
    {
        QString name;
        int covered = 0;
        qint64 estimated = 0;   // somme qty × dernier prix (si connu)
        bool priceComplete = true; // faux si un prix manque (jamais livré)
        int supplied = 0;       // total historique livré sur ces produits
        QSet<int> variants;
    };
    QHash<int, Match> matches;

    for (const QVariant& lineVariant : lines) {
        const QVariantMap line = lineVariant.toMap();
        const int variantId = line.value(QStringLiteral("variantId")).toInt();
        const int qty = line.value(QStringLiteral("qty")).toInt();
        if (variantId <= 0 || qty <= 0)
            continue;
        const auto offers = m_repository.suppliersFor(variantId);
        if (!offers)
            continue;
        for (const SupplierOfferRow& offer : offers.value()) {
            Match& match = matches[offer.supplierId];
            match.name = offer.supplierName;
            match.covered += 1;
            match.variants.insert(variantId);
            match.supplied += offer.qtySupplied;
            if (offer.deliveryCount > 0 && offer.lastCost.millimes() > 0)
                match.estimated += qint64(qty) * offer.lastCost.millimes();
            else
                match.priceComplete = false;
        }
    }

    // Produits manquants par fournisseur (libellés, pour l'affichage).
    const int total = [&lines] {
        int count = 0;
        for (const QVariant& lineVariant : lines)
            if (lineVariant.toMap().value(QStringLiteral("variantId")).toInt() > 0)
                ++count;
        return count;
    }();

    struct Row { int supplierId; Match match; };
    QList<Row> rows;
    for (auto it = matches.constBegin(); it != matches.constEnd(); ++it)
        rows.append({it.key(), it.value()});

    // Tri : couverture d'abord, puis le critère choisi. La distance
    // viendra avec la carte (colonnes lat/lng déjà en base).
    std::sort(rows.begin(), rows.end(), [&sortBy](const Row& a, const Row& b) {
        if (a.match.covered != b.match.covered)
            return a.match.covered > b.match.covered;
        if (sortBy == QLatin1String("quantity")) {
            if (a.match.supplied != b.match.supplied)
                return a.match.supplied > b.match.supplied;
        }
        // "price" : prix complet connu d'abord, total estimé croissant
        if (a.match.priceComplete != b.match.priceComplete)
            return a.match.priceComplete;
        if (a.match.estimated != b.match.estimated)
            return a.match.estimated < b.match.estimated;
        return a.match.name.localeAwareCompare(b.match.name) < 0;
    });

    const QLocale locale;
    QVariantList result;
    for (const Row& row : rows) {
        QStringList missing;
        for (const QVariant& lineVariant : lines) {
            const QVariantMap line = lineVariant.toMap();
            const int variantId = line.value(QStringLiteral("variantId")).toInt();
            if (variantId > 0 && !row.match.variants.contains(variantId))
                missing.append(line.value(QStringLiteral("label")).toString());
        }
        const Money estimated = Money::fromMillimes(row.match.estimated);
        result.append(QVariantMap{
            {QStringLiteral("supplierId"), row.supplierId},
            {QStringLiteral("name"), row.match.name},
            {QStringLiteral("covered"), row.match.covered},
            {QStringLiteral("total"), total},
            {QStringLiteral("full"), row.match.covered == total},
            {QStringLiteral("coverageLabel"),
             QStringLiteral("%1/%2").arg(row.match.covered).arg(total)},
            {QStringLiteral("priceComplete"), row.match.priceComplete},
            {QStringLiteral("estimatedDisplay"),
             row.match.estimated <= 0
                 ? tr("prix inconnu")
                 : (row.match.priceComplete
                        ? QStringLiteral("≈ %1")
                        : QStringLiteral("≥ %1"))
                       .arg(estimated.toDisplayString(locale))},
            {QStringLiteral("suppliedQty"), row.match.supplied},
            {QStringLiteral("missingLabels"),
             missing.join(QStringLiteral(", "))},
        });
    }
    return result;
}

// ── Commandes d'achat (F07-02/03) ─────────────────────────────

bool SupplierController::createOrder(const QVariantMap& data)
{
    PurchaseOrderDraft draft;
    draft.supplierId = data.value(QStringLiteral("supplierId")).toInt();
    if (draft.supplierId <= 0) {
        emit errorOccurred(tr("Choisissez un fournisseur."));
        return false;
    }
    draft.note = data.value(QStringLiteral("note")).toString().trimmed();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;

    const QVariantList lines = data.value(QStringLiteral("lines")).toList();
    for (const QVariant& entry : lines) {
        const QVariantMap map = entry.toMap();
        PoLine line;
        line.variantId = map.value(QStringLiteral("variantId")).toInt();
        line.label = map.value(QStringLiteral("label")).toString();
        line.qtyOrdered = map.value(QStringLiteral("qty")).toInt();
        const qint64 cost =
            Money::parseMillimes(map.value(QStringLiteral("cost")).toString());
        if (cost < 0) {
            emit errorOccurred(tr("Coût invalide — format attendu : 12,500"));
            return false;
        }
        line.unitCost = Money::fromMillimes(cost);
        draft.lines.append(line);
    }
    if (draft.lines.isEmpty()) {
        emit errorOccurred(tr("Ajoutez au moins une ligne."));
        return false;
    }

    const auto created = m_repository.createOrder(draft);
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit orderCreated(created.value().id);
    return true;
}

QVariantMap SupplierController::orderDetail(int poId)
{
    const auto detail = m_repository.orderDetail(poId);
    if (!detail) {
        emit errorOccurred(detail.error().message);
        return {};
    }
    const QLocale locale;
    QVariantList lines;
    for (const PoLine& line : detail.value().lines) {
        lines.append(QVariantMap{
            {QStringLiteral("variantId"), line.variantId},
            {QStringLiteral("label"), line.label},
            {QStringLiteral("qtyOrdered"), line.qtyOrdered},
            {QStringLiteral("qtyReceived"), line.qtyReceived},
            {QStringLiteral("remaining"), line.qtyRemaining()},
            {QStringLiteral("unitCost"), line.unitCost.toDisplayString(locale)},
        });
    }
    return QVariantMap{
        {QStringLiteral("id"), detail.value().id},
        {QStringLiteral("number"), detail.value().number},
        {QStringLiteral("supplierName"), detail.value().supplierName},
        {QStringLiteral("status"), detail.value().status},
        {QStringLiteral("lines"), lines},
    };
}

bool SupplierController::sendOrder(int poId)
{
    const auto sent = m_repository.setOrderStatus(poId, QStringLiteral("sent"));
    if (!sent) {
        emit errorOccurred(sent.error().message);
        return false;
    }
    refresh();
    return true;
}

bool SupplierController::cancelOrder(int poId)
{
    const auto cancelled =
        m_repository.setOrderStatus(poId, QStringLiteral("cancelled"));
    if (!cancelled) {
        emit errorOccurred(cancelled.error().message);
        return false;
    }
    refresh();
    return true;
}

bool SupplierController::isDeletable(int supplierId)
{
    const auto referenced = m_repository.isReferenced(supplierId);
    return referenced.isOk() && !referenced.value();
}

bool SupplierController::deleteSupplier(int supplierId)
{
    const auto removed = m_repository.remove(supplierId);
    if (!removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    refresh();
    emit supplierUpdated();
    return true;
}

bool SupplierController::reopenOrder(int poId)
{
    const auto reopened =
        m_repository.setOrderStatus(poId, QStringLiteral("draft"));
    if (!reopened) {
        emit errorOccurred(reopened.error().message);
        return false;
    }
    refresh();
    return true;
}

bool SupplierController::receiveOrder(const QVariantMap& data)
{
    PoReceiptDraft draft;
    draft.poId = data.value(QStringLiteral("poId")).toInt();
    draft.locationId = data.value(QStringLiteral("locationId")).toInt();
    draft.note = data.value(QStringLiteral("note")).toString().trimmed();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;

    const QVariantList lines = data.value(QStringLiteral("lines")).toList();
    for (const QVariant& entry : lines) {
        const QVariantMap map = entry.toMap();
        PoReceiptLine line;
        line.variantId = map.value(QStringLiteral("variantId")).toInt();
        line.qty = map.value(QStringLiteral("qty")).toInt();
        if (line.qty > 0)
            draft.lines.append(line);
    }
    if (draft.lines.isEmpty()) {
        emit errorOccurred(tr("Saisissez au moins une quantité reçue."));
        return false;
    }

    const auto received = m_repository.receiveOrder(draft);
    if (!received) {
        emit errorOccurred(received.error().message);
        return false;
    }
    refresh();
    emit orderReceived();
    return true;
}

} // namespace nursera
