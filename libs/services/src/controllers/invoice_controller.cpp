#include "controllers/invoice_controller.h"

#include "generators/invoice_generator.h"

#include <QUrl>

#include <utility>

namespace nursera {

InvoiceController::InvoiceController(IInvoiceRepository& invoices,
                                     ISettingsRepository& settings,
                                     QString outputDir,
                                     QObject* parent)
    : QObject(parent)
    , m_invoices(invoices)
    , m_settings(settings)
    , m_outputDir(std::move(outputDir))
{
}

QString InvoiceController::invoiceForSale(int saleId)
{
    const qint64 stampMillimes = Money::parseMillimes(
        m_settings.valueOr(QStringLiteral("finance.stamp_duty"),
                           QStringLiteral("1,000")));
    const Money stamp =
        Money::fromMillimes(stampMillimes >= 0 ? stampMillimes : 0);

    const auto invoice = m_invoices.createFromSale(
        saleId, stamp, m_userIdProvider ? m_userIdProvider() : 0);
    if (!invoice) {
        emit errorOccurred(invoice.error().message);
        return {};
    }

    const auto details = m_invoices.details(invoice.value().id);
    if (!details) {
        emit errorOccurred(details.error().message);
        return {};
    }

    const auto pdf = InvoiceGenerator::generatePdf(
        details.value(), InvoiceGenerator::companyFrom(m_settings), m_outputDir);
    if (!pdf) {
        emit errorOccurred(pdf.error().message);
        return {};
    }
    m_invoices.setPdfPath(invoice.value().id, pdf.value());
    return QUrl::fromLocalFile(pdf.value()).toString();
}

} // namespace nursera
