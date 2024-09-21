#include "controllers/quote_controller.h"

#include "generators/quote_generator.h"

#include <QLocale>
#include <QUrl>

#include <utility>

namespace nursera {
namespace {

QString statusCode(QuoteStatus status)
{
    switch (status) {
    case QuoteStatus::Sent: return QStringLiteral("sent");
    case QuoteStatus::Accepted: return QStringLiteral("accepted");
    case QuoteStatus::Refused: return QStringLiteral("refused");
    case QuoteStatus::Expired: return QStringLiteral("expired");
    case QuoteStatus::Draft: break;
    }
    return QStringLiteral("draft");
}

QString statusLabel(QuoteStatus status)
{
    switch (status) {
    case QuoteStatus::Sent: return QObject::tr("Envoyé");
    case QuoteStatus::Accepted: return QObject::tr("Accepté");
    case QuoteStatus::Refused: return QObject::tr("Refusé");
    case QuoteStatus::Expired: return QObject::tr("Expiré");
    case QuoteStatus::Draft: break;
    }
    return QObject::tr("Brouillon");
}

QuoteStatus statusFromCode(const QString& code)
{
    if (code == QLatin1String("sent")) return QuoteStatus::Sent;
    if (code == QLatin1String("accepted")) return QuoteStatus::Accepted;
    if (code == QLatin1String("refused")) return QuoteStatus::Refused;
    if (code == QLatin1String("expired")) return QuoteStatus::Expired;
    return QuoteStatus::Draft;
}

} // namespace

QuoteController::QuoteController(IQuoteRepository& quotes,
                                 IStockRepository& stock,
                                 ISettingsRepository& settings,
                                 QString outputDir,
                                 QObject* parent)
    : QObject(parent)
    , m_repo(quotes)
    , m_stock(stock)
    , m_settings(settings)
    , m_outputDir(std::move(outputDir))
{
}

void QuoteController::refresh()
{
    const auto list = m_repo.list();
    if (!list) {
        emit errorOccurred(list.error().message);
        return;
    }
    const QLocale locale;
    m_quotes.clear();
    for (const QuoteRow& row : list.value()) {
        m_quotes.append(QVariantMap{
            {QStringLiteral("id"), row.id},
            {QStringLiteral("number"), row.number},
            {QStringLiteral("customerName"),
             row.customerName.isEmpty() ? tr("(sans client)") : row.customerName},
            {QStringLiteral("status"), statusCode(row.status)},
            {QStringLiteral("statusLabel"), statusLabel(row.status)},
            {QStringLiteral("total"), row.total.toDisplayString(locale)},
            {QStringLiteral("createdAt"), row.createdAt.left(10)},
            {QStringLiteral("validUntil"), row.validUntil},
        });
    }
    emit refreshed();
}

QVariantList QuoteController::searchVariants(const QString& term) const
{
    QVariantList picks;
    const auto found = m_stock.searchVariants(term);
    if (!found)
        return picks;
    const QLocale locale;
    for (const VariantPick& pick : found.value()) {
        picks.append(QVariantMap{
            {QStringLiteral("variantId"), pick.variantId},
            {QStringLiteral("label"), pick.label},
            {QStringLiteral("sku"), pick.sku},
            {QStringLiteral("priceMillimes"), pick.priceTtcMillimes},
            {QStringLiteral("priceDisplay"),
             Money::fromMillimes(pick.priceTtcMillimes).toDisplayString(locale)},
        });
    }
    return picks;
}

QVariantList QuoteController::searchCustomers(const QString&) const
{
    return {}; // rempli via le SaleController partagé ; laissé pour extension
}

QVariantMap QuoteController::quoteDetail(int quoteId)
{
    const auto details = m_repo.details(quoteId);
    if (!details) {
        emit errorOccurred(details.error().message);
        return {};
    }
    const QLocale locale;
    QVariantList lines;
    for (const QuoteLine& line : details.value().lines) {
        lines.append(QVariantMap{
            {QStringLiteral("label"), line.label},
            {QStringLiteral("qty"), line.qty},
            {QStringLiteral("isFree"), line.variantId == 0},
            {QStringLiteral("unitPrice"), line.unitPrice.toDisplayString(locale)},
            {QStringLiteral("lineTotal"),
             line.lineTotal().toDisplayString(locale)},
        });
    }
    const QuoteDetails& quote = details.value();
    return QVariantMap{
        {QStringLiteral("id"), quote.id},
        {QStringLiteral("number"), quote.number},
        {QStringLiteral("customerName"),
         quote.customerName.isEmpty() ? tr("(sans client)") : quote.customerName},
        {QStringLiteral("status"), statusCode(quote.status)},
        {QStringLiteral("statusLabel"), statusLabel(quote.status)},
        {QStringLiteral("createdAt"), quote.createdAt.left(10)},
        {QStringLiteral("validUntil"), quote.validUntil},
        {QStringLiteral("note"), quote.note},
        {QStringLiteral("subtotal"), quote.subtotal.toDisplayString(locale)},
        {QStringLiteral("discount"), quote.discount.toDisplayString(locale)},
        {QStringLiteral("hasDiscount"), quote.discount.millimes() > 0},
        {QStringLiteral("total"), quote.total.toDisplayString(locale)},
        {QStringLiteral("lines"), lines},
    };
}

bool QuoteController::createQuote(const QVariantMap& data)
{
    QuoteDraft draft;
    draft.customerId = data.value(QStringLiteral("customerId")).toInt();
    draft.customerName = data.value(QStringLiteral("customerName")).toString().trimmed();
    draft.note = data.value(QStringLiteral("note")).toString().trimmed();
    draft.userId = m_userIdProvider ? m_userIdProvider() : 0;

    const QString discountText = data.value(QStringLiteral("discount")).toString().trimmed();
    if (!discountText.isEmpty()) {
        const qint64 d = Money::parseMillimes(discountText);
        if (d < 0) {
            emit errorOccurred(tr("Remise invalide."));
            return false;
        }
        draft.discount = Money::fromMillimes(d);
    }

    const QVariantList lines = data.value(QStringLiteral("lines")).toList();
    for (const QVariant& entry : lines) {
        const QVariantMap map = entry.toMap();
        QuoteLine line;
        line.variantId = map.value(QStringLiteral("variantId")).toInt();
        line.label = map.value(QStringLiteral("label")).toString().trimmed();
        line.qty = qMax(1, map.value(QStringLiteral("qty")).toInt());
        const qint64 price = Money::parseMillimes(map.value(QStringLiteral("price")).toString());
        if (line.label.isEmpty() || price < 0) {
            emit errorOccurred(tr("Ligne invalide (désignation et prix requis)."));
            return false;
        }
        line.unitPrice = Money::fromMillimes(price);
        draft.lines.append(line);
    }
    if (draft.lines.isEmpty()) {
        emit errorOccurred(tr("Ajoutez au moins une ligne."));
        return false;
    }

    const auto created = m_repo.create(draft);
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit quoteCreated(created.value().id);
    return true;
}

QString QuoteController::pdfFor(int quoteId)
{
    const auto details = m_repo.details(quoteId);
    if (!details) {
        emit errorOccurred(details.error().message);
        return {};
    }
    const auto pdf = QuoteGenerator::generatePdf(
        details.value(), QuoteGenerator::companyFrom(m_settings), m_outputDir);
    if (!pdf) {
        emit errorOccurred(pdf.error().message);
        return {};
    }
    m_repo.setPdfPath(quoteId, pdf.value());
    return QUrl::fromLocalFile(pdf.value()).toString();
}

bool QuoteController::setStatus(int quoteId, const QString& status)
{
    if (const auto result = m_repo.setStatus(quoteId, statusFromCode(status));
        !result) {
        emit errorOccurred(result.error().message);
        return false;
    }
    refresh();
    return true;
}

} // namespace nursera
