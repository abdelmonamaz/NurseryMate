#include "generators/credit_note_generator.h"

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

QString refundLabel(const QString& method)
{
    if (method == QLatin1String("cash"))
        return QStringLiteral("Remboursement en espèces");
    if (method == QLatin1String("cheque"))
        return QStringLiteral("Remboursement par chèque");
    if (method == QLatin1String("transfer"))
        return QStringLiteral("Remboursement par virement");
    if (method == QLatin1String("credit"))
        return QStringLiteral("Imputé sur l'encours du client");
    return method;
}

} // namespace

Result<QString> CreditNoteGenerator::generatePdf(
    const CreditNoteDetails& note,
    const InvoiceGenerator::CompanyInfo& company,
    const QString& outputDir)
{
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("credit.dir"), outputDir);

    const QLocale locale;

    QString html = QStringLiteral(
        "<html><body style='font-family: Inter, sans-serif; color: #45454A;'>");

    // ── En-tête : société (gauche) / avoir (droite) ──
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
                "<span style='font-size: 18pt; font-weight: bold; color: #B3261E;'>"
                "AVOIR</span><br/>"
                "<span style='font-size: 11pt;'><b>%1</b></span><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>Date : %2</span>")
                .arg(esc(note.number), esc(note.createdAt.left(10)));
    html += QStringLiteral("</td></tr></table><hr/>");

    // ── Référence de la vente contre-passée + motif ──
    html += QStringLiteral(
                "<table width='100%'><tr><td style='background:#FDF3F2; padding:8px;'>"
                "<b>Contre-passation de la vente %1</b> du %2"
                "<br/><span style='font-size: 9pt;'>Motif : %3</span>"
                "<br/><span style='font-size: 9pt;'>%4 · "
                "Retour en stock : %5</span>"
                "</td></tr></table><br/>")
                .arg(esc(note.sale.number), esc(note.sale.createdAt.left(10)),
                     esc(note.reason), esc(refundLabel(note.refundMethod)),
                     note.restock ? QStringLiteral("oui") : QStringLiteral("non"));

    // ── Lignes REMBOURSÉES par cet avoir (TTC, en négatif comptable) ──
    html += QStringLiteral(
        "<table width='100%' cellspacing='0' cellpadding='5' border='0' "
        "style='font-size: 10pt;'>"
        "<tr style='background:#45454A; color:white;'>"
        "<th align='left'>Désignation</th><th align='right'>Qté</th>"
        "<th align='right'>P.U. TTC</th><th align='right'>Total TTC</th></tr>");
    int idx = 0;
    for (const SaleDetailLine& line : note.lines) {
        const QString bg = (idx++ % 2) ? QStringLiteral(" style='background:#FAFAFA;'")
                                       : QString();
        html += QStringLiteral(
                    "<tr%1><td>%2</td><td align='right'>%3</td>"
                    "<td align='right'>%4</td><td align='right'><b>−%5</b></td></tr>")
                    .arg(bg, esc(line.label))
                    .arg(line.qty)
                    .arg(line.unitPrice.toDisplayString(locale),
                         line.lineTotal.toDisplayString(locale));
    }
    html += QStringLiteral("</table><hr/>");

    // ── Totaux + montant en lettres ──
    html += QStringLiteral("<table width='100%'><tr>"
                           "<td width='55%' valign='top'>");
    html += QStringLiteral(
                "<div style='background:#FDF3F2; padding:8px; font-size:9pt;'>"
                "Arrêté le présent avoir à la somme de :<br/><b>%1</b></div>")
                .arg(esc(AmountToWords::frenchAmount(note.total)));
    html += QStringLiteral("</td><td width='45%' valign='top'>");
    // Remise de la vente reprise au prorata du remboursé (avoir partiel)
    Money linesSum;
    for (const SaleDetailLine& line : note.lines)
        linesSum = linesSum + line.lineTotal;
    const Money discountShare = linesSum - note.total;
    QString discountRow;
    if (discountShare.millimes() > 0)
        discountRow = QStringLiteral(
                          "<tr><td>Remise reprise</td>"
                          "<td align='right'>%1</td></tr>")
                          .arg(discountShare.toDisplayString(locale));
    html += QStringLiteral(
        "<table width='100%' cellpadding='4' style='font-size:10pt;'>"
        "%1"
        "<tr style='font-size:13pt; color:#B3261E;'>"
        "<td><b>Total avoir</b></td><td align='right'><b>−%2</b></td></tr>"
        "</table>")
                .arg(discountRow, note.total.toDisplayString(locale));
    html += QStringLiteral("</td></tr></table>");

    html += QStringLiteral(
        "<br/><br/><p align='center' style='font-size:8pt; color:#71767C;'>"
        "Avoir émis par %1 sur la vente %2 · Document comptable — à conserver"
        "</p></body></html>")
                .arg(esc(note.userName.isEmpty() ? QStringLiteral("—")
                                                 : note.userName),
                     esc(note.sale.number));

    QTextDocument document;
    if (!logo.isNull())
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("logo")), logo);
    document.setHtml(html);

    const QString path = outputDir + QLatin1Char('/') + note.number
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
