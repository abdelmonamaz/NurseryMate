#include "generators/quote_generator.h"

#include "common/amount_to_words.h"

#include <QDir>
#include <QImage>
#include <QLocale>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>
#include <QUrl>

namespace nursera {
namespace {

QString esc(const QString& text) { return text.toHtmlEscaped(); }

} // namespace

QuoteGenerator::CompanyInfo QuoteGenerator::companyFrom(ISettingsRepository& settings)
{
    CompanyInfo company;
    company.name = settings.valueOr(QStringLiteral("company.name"),
                                    QStringLiteral("Pépinière Idéale"));
    company.tagline = settings.valueOr(QStringLiteral("company.tagline"),
                                       QStringLiteral("Vente, Aménagement & Entretien"));
    company.address = settings.valueOr(QStringLiteral("company.address"));
    company.phone = settings.valueOr(QStringLiteral("company.phone"));
    company.taxId = settings.valueOr(QStringLiteral("company.tax_id"));
    return company;
}

Result<QString> QuoteGenerator::generatePdf(const QuoteDetails& quote,
                                            const CompanyInfo& company,
                                            const QString& outputDir)
{
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("quote.dir"), outputDir);

    const QLocale locale;
    QString html = QStringLiteral(
        "<html><body style='font-family: Inter, sans-serif; color: #45454A;'>");

    const QImage logo(QStringLiteral(":/resources/logo.jpeg"));
    html += QStringLiteral("<table width='100%'><tr>"
                           "<td width='60%' valign='top'>");
    if (!logo.isNull())
        html += QStringLiteral("<img src='logo' width='56'/><br/>");
    html += QStringLiteral(
                "<span style='font-size: 15pt; font-weight: bold;'>%1</span><br/>"
                "<span style='font-size: 8pt; color: #71767C;'>%2</span>")
                .arg(esc(company.name), esc(company.tagline));
    if (!company.address.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>%1</span>")
                    .arg(esc(company.address));
    if (!company.phone.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>Tél : %1</span>")
                    .arg(esc(company.phone));
    if (!company.taxId.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>MF : %1</span>")
                    .arg(esc(company.taxId));
    html += QStringLiteral(
                "</td><td width='40%' valign='top' align='right'>"
                "<span style='font-size: 18pt; font-weight: bold; color: #3E7D14;'>"
                "DEVIS</span><br/>"
                "<span style='font-size: 11pt;'><b>%1</b></span><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>Date : %2<br/>"
                "Valable jusqu'au : %3</span>")
                .arg(esc(quote.number), esc(quote.createdAt.left(10)),
                     esc(quote.validUntil));
    html += QStringLiteral("</td></tr></table><hr/>");

    html += QStringLiteral(
        "<table width='100%'><tr><td style='background:#F0F5EA; padding:8px;'>"
        "<b>Client :</b> %1")
                .arg(esc(quote.customerName.isEmpty() ? QStringLiteral("—")
                                                      : quote.customerName));
    if (!quote.customerTaxId.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>MF : %1</span>")
                    .arg(esc(quote.customerTaxId));
    html += QStringLiteral("</td></tr></table><br/>");

    html += QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='5' style='font-size: 10pt;'>"
        "<tr style='background:#3E7D14; color:white;'>"
        "<th align='left'>Désignation</th><th align='right'>Qté</th>"
        "<th align='right'>P.U.</th><th align='right'>Total</th></tr>");
    int idx = 0;
    for (const QuoteLine& line : quote.lines) {
        const QString bg = (idx++ % 2) ? QStringLiteral(" style='background:#FAFBF7;'")
                                       : QString();
        html += QStringLiteral(
                    "<tr%1><td>%2</td><td align='right'>%3</td>"
                    "<td align='right'>%4</td><td align='right'><b>%5</b></td></tr>")
                    .arg(bg, esc(line.label))
                    .arg(line.qty)
                    .arg(line.unitPrice.toDisplayString(locale),
                         line.lineTotal().toDisplayString(locale));
    }
    html += QStringLiteral("</table><hr/>");

    html += QStringLiteral("<table width='100%'><tr>"
                           "<td width='55%' valign='top'>"
                           "<div style='background:#F0F5EA; padding:8px; font-size:9pt;'>"
                           "Arrêté le présent devis à la somme de :<br/><b>%1</b></div>")
                .arg(esc(AmountToWords::frenchAmount(quote.total)));
    if (!quote.note.isEmpty())
        html += QStringLiteral("<p style='font-size:8pt; color:#71767C;'>%1</p>")
                    .arg(esc(quote.note));
    html += QStringLiteral("</td><td width='45%' valign='top'>"
                           "<table width='100%' cellpadding='4' style='font-size:10pt;'>");
    if (quote.discount.millimes() > 0)
        html += QStringLiteral(
                    "<tr><td>Sous-total</td><td align='right'>%1</td></tr>"
                    "<tr><td>Remise</td><td align='right'>−%2</td></tr>")
                    .arg(quote.subtotal.toDisplayString(locale),
                         quote.discount.toDisplayString(locale));
    html += QStringLiteral(
                "<tr style='font-size:13pt; color:#3E7D14;'>"
                "<td><b>TOTAL</b></td><td align='right'><b>%1</b></td></tr>"
                "</table></td></tr></table>")
                .arg(quote.total.toDisplayString(locale));

    html += QStringLiteral(
        "<br/><br/><p style='font-size:8pt; color:#71767C;'>"
        "Devis non contractuel · Prix susceptibles de varier après la date de "
        "validité 🌱</p></body></html>");

    QTextDocument document;
    if (!logo.isNull())
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("logo")), logo);
    document.setHtml(html);

    const QString path = outputDir + QLatin1Char('/') + quote.number
        + QStringLiteral(".pdf");
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setResolution(150);
    document.setPageSize(QSizeF(writer.width(), writer.height()));
    document.print(&writer);

    return Result<QString>::ok(path);
}

} // namespace nursera
