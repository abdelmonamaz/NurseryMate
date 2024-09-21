#include "controllers/sale_controller.h"

#include "generators/credit_note_generator.h"
#include "generators/ticket_generator.h"

#include <QLocale>
#include <QUrl>

namespace nursera {

SaleController::SaleController(ISaleRepository& sales,
                               IStockRepository& stock,
                               ILocationRepository& locations,
                               QObject* parent)
    : QObject(parent)
    , m_sales(sales)
    , m_stock(stock)
    , m_locations(locations)
{
    connect(&m_cart, &QAbstractItemModel::dataChanged, this,
            &SaleController::cartChanged);
    connect(&m_cart, &QAbstractItemModel::rowsInserted, this,
            &SaleController::cartChanged);
    connect(&m_cart, &QAbstractItemModel::rowsRemoved, this,
            &SaleController::cartChanged);
    connect(&m_cart, &QAbstractItemModel::modelReset, this,
            &SaleController::cartChanged);
}

QString SaleController::subtotalDisplay() const
{
    return m_cart.subtotal().toDisplayString(QLocale());
}

QString SaleController::discountDisplay() const
{
    return (-m_globalDiscount).toDisplayString(QLocale());
}

QString SaleController::totalDisplay() const
{
    return total().toDisplayString(QLocale());
}

// La caisse décrémente le premier emplacement "zone de vente" (RG-04.a)
// — configurable via les paramètres en V1.
int SaleController::salesLocationId() const
{
    const auto locations = m_locations.all();
    if (!locations)
        return 0;
    for (const Location& location : locations.value()) {
        if (location.kind == LocationKind::SalesArea)
            return location.id;
    }
    // Repli : premier emplacement actif
    return locations.value().isEmpty() ? 0 : locations.value().first().id;
}

QVariantList SaleController::searchVariants(const QString& term) const
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
            {QStringLiteral("priceMillimes"), pick.priceTtcMillimes},
            {QStringLiteral("priceProMillimes"), pick.priceProMillimes},
            {QStringLiteral("priceDisplay"),
             Money::fromMillimes(pick.priceTtcMillimes).toDisplayString(QLocale())},
            {QStringLiteral("vatRate"), pick.vatRatePercent},
        });
    }
    return picks;
}

QVariantList SaleController::searchCustomers(const QString& term) const
{
    QVariantList rows;
    if (!m_customers)
        return rows;
    const auto found = m_customers->search(term);
    if (!found)
        return rows;
    const QLocale locale;
    for (const CustomerRow& row : found.value()) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("name"), row.name},
            {QStringLiteral("phone"), row.phone},
            {QStringLiteral("balanceDisplay"), row.balance.toDisplayString(locale)},
            {QStringLiteral("hasDebt"), row.balance.millimes() > 0},
        });
    }
    return rows;
}

void SaleController::setCartCustomer(int customerId)
{
    m_cartCustomerId = customerId;
    m_cartCustomerName.clear();
    m_cartIsPro = false;

    if (customerId > 0 && m_customers) {
        if (const auto customer = m_customers->byId(customerId)) {
            m_cartCustomerName = customer.value().name;
            m_cartIsPro = customer.value().kind == CustomerKind::Professional;
        }
    }
    // Bascule tarif particulier / pro (RG-04.c)
    m_cart.repriceForPro(m_cartIsPro);
    emit customerChanged();
    emit cartChanged();
}

void SaleController::addToCart(const QVariantMap& pick)
{
    SaleLine line;
    line.variantId = pick.value(QStringLiteral("variantId")).toInt();
    line.label = pick.value(QStringLiteral("label")).toString();
    line.qty = qMax(1, pick.value(QStringLiteral("qty"), 1).toInt());
    line.unitPriceRegular = Money::fromMillimes(
        pick.value(QStringLiteral("priceMillimes")).toLongLong());
    line.unitPricePro = Money::fromMillimes(
        pick.value(QStringLiteral("priceProMillimes")).toLongLong());
    line.vatRatePercent = pick.value(QStringLiteral("vatRate")).toInt();
    // Tarif effectif selon le client du panier (RG-04.c)
    line.unitPrice = (m_cartIsPro && line.unitPricePro.millimes() > 0)
        ? line.unitPricePro
        : line.unitPriceRegular;
    if (line.variantId <= 0)
        return;
    m_cart.addLine(line);
}

void SaleController::setQty(int row, int qty)
{
    m_cart.setQty(row, qty);
}

bool SaleController::setLinePrice(int row, const QString& priceText)
{
    const qint64 millimes = Money::parseMillimes(priceText);
    if (millimes < 0) {
        emit errorOccurred(tr("Prix invalide — format attendu : 12,500"));
        return false;
    }
    m_cart.setUnitPrice(row, Money::fromMillimes(millimes));
    emit cartChanged();
    return true;
}

void SaleController::removeAt(int row)
{
    m_cart.removeAt(row);
}

void SaleController::clearCart()
{
    m_cart.clear();
    m_globalDiscount = Money{};
    m_cartCustomerId = 0;
    m_cartCustomerName.clear();
    m_cartIsPro = false;
    emit cartChanged();
    emit customerChanged();
}

QVariantList SaleController::heldCarts() const
{
    QVariantList list;
    const QLocale locale;
    for (int i = 0; i < m_held.size(); ++i) {
        const HeldCart& held = m_held.at(i);
        Money total;
        int items = 0;
        for (const SaleLine& line : held.lines) {
            total = total + line.lineTotal();
            items += line.qty;
        }
        total = total - held.discount;
        list.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("label"), held.customerId > 0
                ? held.customerName
                : tr("Panier %1").arg(i + 1)},
            {QStringLiteral("itemCount"), items},
            {QStringLiteral("total"), total.toDisplayString(locale)},
        });
    }
    return list;
}

bool SaleController::loadQuote(int quoteId)
{
    if (!m_quotes) {
        emit errorOccurred(tr("Devis indisponibles."));
        return false;
    }
    const auto details = m_quotes->details(quoteId);
    if (!details) {
        emit errorOccurred(details.error().message);
        return false;
    }
    if (details.value().status != QuoteStatus::Accepted) {
        emit errorOccurred(
            tr("Seul un devis accepté peut être chargé en caisse."));
        return false;
    }

    // Le panier en cours part en attente — rien ne se perd.
    holdCart();

    QList<SaleLine> lines;
    for (const QuoteLine& quoteLine : details.value().lines) {
        SaleLine line;
        line.variantId = quoteLine.variantId; // 0 = prestation libre
        line.label = quoteLine.label;
        line.qty = quoteLine.qty;
        line.unitPrice = quoteLine.unitPrice;
        line.unitPriceRegular = quoteLine.unitPrice;
        // Prix du devis = engagement commercial : la bascule tarif pro
        // ne doit pas l'écraser.
        line.manualPrice = true;
        lines.append(line);
    }
    m_cart.setLines(std::move(lines));
    setGlobalDiscount(details.value().discount.millimes() > 0
                          ? QString::number(
                                details.value().discount.millimes() / 1000.0,
                                'f', 3)
                                .replace(QLatin1Char('.'), QLatin1Char(','))
                          : QString());
    // Client du devis rattaché (bascule pro sans écraser les prix : les
    // lignes sont manualPrice).
    setCartCustomer(details.value().customerId);

    emit cartChanged();
    return true;
}

void SaleController::holdCart()
{
    if (m_cart.lines().isEmpty())
        return;
    m_held.append(HeldCart{m_cart.lines(), m_globalDiscount, m_cartCustomerId,
                           m_cartCustomerName, m_cartIsPro});
    clearCart();
    emit heldChanged();
}

void SaleController::resumeCart(int index)
{
    if (index < 0 || index >= m_held.size())
        return;
    // Ne pas perdre le panier courant : le mettre en attente
    if (!m_cart.lines().isEmpty())
        holdCart();

    const HeldCart held = m_held.takeAt(index);
    m_cart.setLines(held.lines);
    m_globalDiscount = held.discount;
    m_cartCustomerId = held.customerId;
    m_cartCustomerName = held.customerName;
    m_cartIsPro = held.isPro;
    emit cartChanged();
    emit customerChanged();
    emit heldChanged();
}

bool SaleController::setGlobalDiscount(const QString& amount)
{
    if (amount.trimmed().isEmpty()) {
        m_globalDiscount = Money{};
        emit cartChanged();
        return true;
    }
    const qint64 millimes = Money::parseMillimes(amount);
    if (millimes < 0) {
        emit errorOccurred(tr("Remise invalide — format attendu : 3,500"));
        return false;
    }
    if (Money::fromMillimes(millimes) > m_cart.subtotal()) {
        emit errorOccurred(tr("La remise dépasse le total."));
        return false;
    }
    m_globalDiscount = Money::fromMillimes(millimes);
    emit cartChanged();
    return true;
}

QString SaleController::changeFor(const QString& amountGiven) const
{
    const qint64 given = Money::parseMillimes(amountGiven);
    if (given < 0)
        return {};
    const Money change = Money::fromMillimes(given) - total();
    if (change.isNegative())
        return {};
    return change.toDisplayString(QLocale());
}

bool SaleController::checkout(const QVariantMap& data)
{
    SaleDraft draft;
    draft.lines = m_cart.lines();
    draft.globalDiscount = m_globalDiscount;
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;
    draft.stockLocationId = salesLocationId();

    const QString method = data.value(QStringLiteral("method")).toString();
    draft.method = method == QLatin1String("cheque") ? PaymentMethod::Cheque
        : method == QLatin1String("transfer") ? PaymentMethod::Transfer
                                              : PaymentMethod::Cash;
    draft.chequeNumber = data.value(QStringLiteral("chequeNumber")).toString().trimmed();
    draft.chequeBank = data.value(QStringLiteral("chequeBank")).toString().trimmed();
    // Client déjà rattaché au panier (peut être surchargé au paiement)
    draft.customerId = data.contains(QStringLiteral("customerId"))
        ? data.value(QStringLiteral("customerId")).toInt()
        : m_cartCustomerId;
    draft.onCredit = method == QLatin1String("credit");

    if (draft.method == PaymentMethod::Cheque && draft.chequeNumber.isEmpty()) {
        emit errorOccurred(tr("Le numéro de chèque est obligatoire."));
        return false;
    }
    if (draft.onCredit && draft.customerId <= 0) {
        emit errorOccurred(tr("Choisissez un client pour une vente à crédit."));
        return false;
    }

    const auto recorded = m_sales.record(draft);
    if (!recorded) {
        emit errorOccurred(recorded.error().message);
        return false;
    }

    const QString totalText = recorded.value().total.toDisplayString(QLocale());
    const QString ticketUrl = ticketFor(recorded.value().id);
    clearCart();
    emit saleCompleted(recorded.value().number, totalText, ticketUrl);
    return true;
}

QString SaleController::ticketFor(int saleId)
{
    if (!m_settings || m_ticketDir.isEmpty())
        return {};

    const auto details = m_sales.details(saleId);
    if (!details) {
        emit errorOccurred(details.error().message);
        return {};
    }

    const auto pdf = TicketGenerator::generatePdf(
        details.value(), TicketGenerator::companyFrom(*m_settings), m_ticketDir);
    if (!pdf) {
        emit errorOccurred(pdf.error().message);
        return {};
    }
    return QUrl::fromLocalFile(pdf.value()).toString();
}

bool SaleController::cancelSale(int saleId, const QString& reason)
{
    const auto cancelled = m_sales.cancel(
        saleId, reason, m_userIdProvider ? m_userIdProvider() : 0);
    if (!cancelled) {
        emit errorOccurred(cancelled.error().message);
        return false;
    }
    emit saleCancelled();
    return true;
}

bool SaleController::createCreditNote(const QVariantMap& data)
{
    CreditNoteDraft draft;
    draft.saleId = data.value(QStringLiteral("saleId")).toInt();
    draft.reason = data.value(QStringLiteral("reason")).toString().trimmed();
    draft.restock = data.value(QStringLiteral("restock"), true).toBool();
    draft.refundMethod =
        data.value(QStringLiteral("refundMethod"),
                   QStringLiteral("cash")).toString();
    draft.stockLocationId = data.value(QStringLiteral("locationId")).toInt();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;
    // Avoir partiel : lignes et quantités choisies (absent = total)
    const QVariantList lines = data.value(QStringLiteral("lines")).toList();
    for (const QVariant& lineVariant : lines) {
        const QVariantMap line = lineVariant.toMap();
        CreditNoteLineDraft lineDraft;
        lineDraft.saleLineId = line.value(QStringLiteral("saleLineId")).toInt();
        lineDraft.qty = line.value(QStringLiteral("qty")).toInt();
        draft.lines.append(lineDraft);
    }

    const auto note = m_sales.createCreditNote(draft);
    if (!note) {
        emit errorOccurred(note.error().message);
        return false;
    }
    emit creditNoteCreated(note.value().number,
                           creditNotePdfFor(draft.saleId));
    emit saleCancelled(); // rafraîchit journal et stock côté écrans
    return true;
}

QString SaleController::creditNotePdfFor(int saleId)
{
    if (!m_settings || m_creditNoteDir.isEmpty())
        return {};

    const auto details = m_sales.creditNoteDetails(saleId);
    if (!details) {
        emit errorOccurred(details.error().message);
        return {};
    }

    const auto pdf = CreditNoteGenerator::generatePdf(
        details.value(), InvoiceGenerator::companyFrom(*m_settings),
        m_creditNoteDir);
    if (!pdf) {
        emit errorOccurred(pdf.error().message);
        return {};
    }
    return QUrl::fromLocalFile(pdf.value()).toString();
}

QVariantList SaleController::todayJournal() const
{
    QVariantList rows;
    const auto journal = m_sales.todayJournal();
    if (!journal)
        return rows;
    for (const SaleJournalRow& row : journal.value()) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("number"), row.number},
            {QStringLiteral("time"), row.createdAt.mid(11, 5)},
            {QStringLiteral("total"), row.total.toDisplayString(QLocale())},
            {QStringLiteral("method"), row.method},
            {QStringLiteral("userName"), row.userName},
            {QStringLiteral("cancelled"),
             row.status == QLatin1String("cancelled")},
            {QStringLiteral("creditNote"), row.creditNoteNumber},
            {QStringLiteral("creditNoteCount"), row.creditNoteCount},
            // Encore remboursable ? (avoir partiel — cumul < total)
            {QStringLiteral("refundable"),
             row.status != QLatin1String("cancelled")
                 && row.refundedTotal.millimes() < row.total.millimes()},
        });
    }
    return rows;
}

QVariantList SaleController::refundableLines(int saleId) const
{
    QVariantList rows;
    const auto lines = m_sales.refundableLines(saleId);
    if (!lines)
        return rows;
    const QLocale locale;
    for (const RefundableLine& line : lines.value()) {
        rows.append(QVariantMap{
            {QStringLiteral("saleLineId"), line.saleLineId},
            {QStringLiteral("label"), line.label},
            {QStringLiteral("qtySold"), line.qtySold},
            {QStringLiteral("qtyRefunded"), line.qtyRefunded},
            {QStringLiteral("remaining"), line.remaining()},
            {QStringLiteral("unitPriceDisplay"),
             line.unitPrice.toDisplayString(locale)},
        });
    }
    return rows;
}

QString SaleController::expectedCashDisplay() const
{
    const auto totals = m_sales.todayTotals();
    return totals ? totals.value().cash.toDisplayString(QLocale())
                  : QStringLiteral("0,000 DT");
}

bool SaleController::recordClosure(const QString& countedAmount)
{
    const qint64 counted = Money::parseMillimes(countedAmount);
    if (counted < 0) {
        emit errorOccurred(tr("Montant compté invalide."));
        return false;
    }
    const auto gap = m_sales.recordCashClosure(
        Money::fromMillimes(counted), m_userIdProvider ? m_userIdProvider() : 0);
    if (!gap) {
        emit errorOccurred(gap.error().message);
        return false;
    }
    const Money value = gap.value();
    const QString display = (value.millimes() > 0 ? QStringLiteral("+") : QString())
        + value.toDisplayString(QLocale());
    emit closureDone(display, value.millimes() != 0);
    return true;
}

QVariantMap SaleController::todayTotals() const
{
    const auto totals = m_sales.todayTotals();
    if (!totals)
        return {};
    const QLocale locale;
    return QVariantMap{
        {QStringLiteral("count"), totals.value().saleCount},
        {QStringLiteral("total"), totals.value().total.toDisplayString(locale)},
        {QStringLiteral("cash"), totals.value().cash.toDisplayString(locale)},
        {QStringLiteral("cheque"), totals.value().cheque.toDisplayString(locale)},
        {QStringLiteral("transfer"), totals.value().transfer.toDisplayString(locale)},
    };
}

} // namespace nursera
