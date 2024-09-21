#include "generators/invoice_generator.h"

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

QString esc(const QString& text)
{
    return text.toHtmlEscaped();
}

} // namespace

InvoiceGenerator::CompanyInfo
InvoiceGenerator::companyFrom(ISettingsRepository& settings)
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

Result<QString> InvoiceGenerator::generatePdf(const InvoiceDetails& invoice,
                                              const CompanyInfo& company,
                                              const QString& outputDir)
{
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("invoice.dir"), outputDir);

    const QLocale locale;
    const Invoice& h = invoice.header;

    QString html = QStringLiteral(
        "<html><body style='font-family: Inter, sans-serif; color: #45454A;'>");

    // ── En-tête : société (gauche) / facture (droite) ──
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
                "FACTURE</span><br/>"
                "<span style='font-size: 11pt;'><b>%1</b></span><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>Date : %2</span>")
                .arg(esc(h.number), esc(h.issuedAt.left(10)));
    html += QStringLiteral("</td></tr></table><hr/>");

    // ── Client ──
    html += QStringLiteral(
        "<table width='100%'><tr><td style='background:#F0F5EA; padding:8px;'>"
        "<b>Client :</b> %1")
                .arg(esc(h.customerName.isEmpty() ? QStringLiteral("Client de passage")
                                                  : h.customerName));
    if (!h.customerTaxId.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>MF : %1</span>")
                    .arg(esc(h.customerTaxId));
    html += QStringLiteral("</td></tr></table><br/>");

    // ── Lignes HT ──
    html += QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='5' border='0' "
        "style='font-size: 10pt;'>"
        "<tr style='background:#3E7D14; color:white;'>"
        "<th align='left'>Désignation</th><th align='right'>Qté</th>"
        "<th align='right'>P.U. HT</th><th align='right'>TVA</th>"
        "<th align='right'>Total HT</th></tr>");
    int idx = 0;
    for (const SaleDetailLine& line : invoice.lines) {
        // Retrouver le taux de la ligne via le récap n'est pas direct ;
        // on affiche le taux stocké dans vatRows si unique, sinon vide.
        const QString bg = (idx++ % 2) ? QStringLiteral(" style='background:#FAFBF7;'")
                                       : QString();
        html += QStringLiteral(
                    "<tr%1><td>%2</td><td align='right'>%3</td>"
                    "<td align='right'>%4</td><td align='right'>%5</td>"
                    "<td align='right'><b>%6</b></td></tr>")
                    .arg(bg, esc(line.label))
                    .arg(line.qty)
                    .arg(line.unitPrice.toDisplayString(locale),
                         invoice.vatRows.size() == 1
                             ? QStringLiteral("%1 %").arg(invoice.vatRows.first().ratePercent)
                             : QStringLiteral("—"),
                         line.lineTotal.toDisplayString(locale));
    }
    html += QStringLiteral("</table><hr/>");

    // ── Récap TVA par taux + totaux ──
    html += QStringLiteral("<table width='100%'><tr>"
                           "<td width='55%' valign='top'>");
    // Montant en lettres (F05-03)
    html += QStringLiteral(
                "<div style='background:#F0F5EA; padding:8px; font-size:9pt;'>"
                "Arrêtée la présente facture à la somme de :<br/><b>%1</b></div>")
                .arg(esc(AmountToWords::frenchAmount(h.total)));

    if (invoice.vatRows.size() > 1) {
        html += QStringLiteral(
            "<br/><table width='100%' cellpadding='3' style='font-size:8pt;'>"
            "<tr style='color:#71767C;'><th align='left'>Taux</th>"
            "<th align='right'>Base HT</th><th align='right'>TVA</th></tr>");
        for (const VatBreakdownRow& row : invoice.vatRows)
            html += QStringLiteral(
                        "<tr><td>%1 %</td><td align='right'>%2</td>"
                        "<td align='right'>%3</td></tr>")
                        .arg(row.ratePercent)
                        .arg(row.baseHt.toDisplayString(locale),
                             row.vat.toDisplayString(locale));
        html += QStringLiteral("</table>");
    }

    html += QStringLiteral("</td><td width='45%' valign='top'>");
    html += QStringLiteral(
        "<table width='100%' cellpadding='4' style='font-size:10pt;'>"
        "<tr><td>Total HT</td><td align='right'>%1</td></tr>"
        "<tr><td>Total TVA</td><td align='right'>%2</td></tr>"
        "<tr><td>Timbre fiscal</td><td align='right'>%3</td></tr>"
        "<tr style='font-size:13pt; color:#3E7D14;'>"
        "<td><b>Total TTC</b></td><td align='right'><b>%4</b></td></tr>"
        "</table>")
                .arg(h.subtotalHt.toDisplayString(locale),
                     h.vatTotal.toDisplayString(locale),
                     h.stampDuty.toDisplayString(locale),
                     h.total.toDisplayString(locale));
    html += QStringLiteral("</td></tr></table>");

    html += QStringLiteral(
        "<br/><br/><p align='center' style='font-size:8pt; color:#71767C;'>"
        "Facture émise depuis le ticket %1 · Merci de votre confiance 🌱"
        "</p></body></html>").arg(esc(invoice.saleNumber));

    QTextDocument document;
    if (!logo.isNull())
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("logo")), logo);
    document.setHtml(html);

    const QString path = outputDir + QLatin1Char('/') + h.number
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
