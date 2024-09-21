#include "generators/ticket_generator.h"

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

QString escaped(const QString& text)
{
    return text.toHtmlEscaped();
}

} // namespace

TicketGenerator::CompanyInfo TicketGenerator::companyFrom(ISettingsRepository& settings)
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

Result<QString> TicketGenerator::generatePdf(const SaleDetails& sale,
                                             const CompanyInfo& company,
                                             const QString& outputDir)
{
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("ticket.dir"), outputDir);

    const QLocale locale;
    QString html = QStringLiteral(
        "<html><body style='font-family: Inter, sans-serif; color: #45454A;'>");

    // ── En-tête société ──
    const QImage logo(QStringLiteral(":/resources/logo.jpeg"));
    html += QStringLiteral("<table width='100%'><tr>");
    if (!logo.isNull())
        html += QStringLiteral("<td width='70'><img src='logo' width='64'/></td>");
    html += QStringLiteral(
                "<td><span style='font-size: 16pt; font-weight: bold;'>%1</span><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>%2</span>")
                .arg(escaped(company.name), escaped(company.tagline));
    if (!company.address.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>%1</span>")
                    .arg(escaped(company.address));
    if (!company.phone.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>Tél : %1</span>")
                    .arg(escaped(company.phone));
    if (!company.taxId.isEmpty())
        html += QStringLiteral("<br/><span style='font-size: 8pt;'>MF : %1</span>")
                    .arg(escaped(company.taxId));
    html += QStringLiteral("</td></tr></table><hr/>");

    // ── Entête ticket ──
    html += QStringLiteral(
                "<p style='font-size: 12pt;'><b>Ticket %1</b><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>%2 · Vendeur : %3</span></p>")
                .arg(escaped(sale.number),
                     escaped(sale.createdAt),
                     escaped(sale.userName.isEmpty() ? QStringLiteral("—")
                                                     : sale.userName));

    // ── Lignes ──
    html += QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='4' style='font-size: 10pt;'>"
        "<tr style='color: #71767C; font-size: 8pt;'>"
        "<th align='left'>Article</th><th align='right'>Qté</th>"
        "<th align='right'>P.U.</th><th align='right'>Total</th></tr>");
    for (const SaleDetailLine& line : sale.lines) {
        html += QStringLiteral(
                    "<tr><td>%1</td><td align='right'>%2</td>"
                    "<td align='right'>%3</td><td align='right'><b>%4</b></td></tr>")
                    .arg(escaped(line.label))
                    .arg(line.qty)
                    .arg(line.unitPrice.toDisplayString(locale),
                         line.lineTotal.toDisplayString(locale));
    }
    html += QStringLiteral("</table><hr/>");

    // ── Totaux ──
    html += QStringLiteral("<table width='100%' style='font-size: 10pt;'>");
    if (sale.discount.millimes() > 0) {
        html += QStringLiteral(
                    "<tr><td>Sous-total</td><td align='right'>%1</td></tr>"
                    "<tr><td>Remise</td><td align='right'>−%2</td></tr>")
                    .arg(sale.subtotal.toDisplayString(locale),
                         sale.discount.toDisplayString(locale));
    }
    html += QStringLiteral(
                "<tr><td style='font-size: 14pt;'><b>TOTAL</b></td>"
                "<td align='right' style='font-size: 14pt;'><b>%1</b></td></tr>"
                "<tr><td colspan='2' style='font-size: 8pt; color: #71767C;'>"
                "dont TVA : %2 · Paiement : %3</td></tr></table>")
                .arg(sale.total.toDisplayString(locale),
                     sale.vatTotal.toDisplayString(locale),
                     escaped(sale.method));

    html += QStringLiteral(
        "<hr/><p align='center' style='font-size: 9pt; color: #71767C;'>"
        "Merci de votre visite · شكرا لزيارتكم 🌱</p></body></html>");

    QTextDocument document;
    if (!logo.isNull())
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("logo")), logo);
    document.setHtml(html);

    const QString path = outputDir + QLatin1Char('/') + sale.number
        + QStringLiteral(".pdf");
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A5));
    writer.setPageMargins(QMarginsF(12, 12, 12, 12), QPageLayout::Millimeter);
    writer.setResolution(150);
    document.setPageSize(QSizeF(writer.width(), writer.height()));
    document.print(&writer);

    return Result<QString>::ok(path);
}

} // namespace nursera
