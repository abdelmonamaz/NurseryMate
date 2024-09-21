#include "generators/report_generator.h"

#include <QDate>
#include <QDir>
#include <QImage>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>
#include <QUrl>

namespace nursera {
namespace {

QString esc(const QString& text) { return text.toHtmlEscaped(); }

} // namespace

ReportGenerator::CompanyInfo ReportGenerator::companyFrom(
    ISettingsRepository& settings)
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

Result<QString> ReportGenerator::generatePdf(const QString& title,
                                             const QString& periodText,
                                             const QList<Table>& tables,
                                             const CompanyInfo& company,
                                             const QString& outputDir,
                                             const QString& fileName)
{
    if (!QDir().mkpath(outputDir))
        return Result<QString>::fail(QStringLiteral("report.dir"), outputDir);

    QString html = QStringLiteral(
        "<html><body style='font-family: Inter, sans-serif; color: #45454A;'>");

    // En-tête société + titre du rapport
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
                "<span style='font-size: 16pt; font-weight: bold; color: #3E7D14;'>"
                "%1</span><br/>"
                "<span style='font-size: 9pt; color: #71767C;'>%2<br/>"
                "Édité le %3</span>")
                .arg(esc(title), esc(periodText),
                     esc(QDate::currentDate().toString(Qt::ISODate)));
    html += QStringLiteral("</td></tr></table><hr/>");

    // Tableaux (titre omis s'il répète celui du rapport)
    for (const Table& table : tables) {
        if (!table.title.isEmpty() && table.title != title)
            html += QStringLiteral(
                        "<br/><span style='font-size: 12pt; font-weight: bold;'>%1"
                        "</span>")
                        .arg(esc(table.title));
        html += QStringLiteral("<br/><br/>");

        if (table.rows.isEmpty()) {
            html += QStringLiteral(
                "<p style='font-size: 9pt; color: #71767C;'>"
                "Aucune donnée sur la période.</p>");
            continue;
        }

        html += QStringLiteral(
            "<table width='100%' cellspacing='0' cellpadding='5' "
            "style='font-size: 10pt;'>"
            "<tr style='background:#3E7D14; color:white;'>");
        for (int c = 0; c < table.headers.size(); ++c)
            html += QStringLiteral("<th align='%1'>%2</th>")
                        .arg(c == 0 ? QStringLiteral("left")
                                    : QStringLiteral("right"),
                             esc(table.headers.at(c)));
        html += QStringLiteral("</tr>");

        int idx = 0;
        for (const QStringList& row : table.rows) {
            const QString bg = (idx++ % 2)
                ? QStringLiteral(" style='background:#FAFBF7;'")
                : QString();
            html += QStringLiteral("<tr%1>").arg(bg);
            for (int c = 0; c < row.size(); ++c)
                html += QStringLiteral("<td align='%1'>%2</td>")
                            .arg(c == 0 ? QStringLiteral("left")
                                        : QStringLiteral("right"),
                                 esc(row.at(c)));
            html += QStringLiteral("</tr>");
        }
        html += QStringLiteral("</table>");

        if (!table.footer.isEmpty())
            html += QStringLiteral(
                        "<p align='right' style='font-size: 12pt; "
                        "color:#3E7D14;'><b>%1</b></p>")
                        .arg(esc(table.footer));
    }

    html += QStringLiteral(
        "<br/><p style='font-size:8pt; color:#71767C;'>"
        "Document interne — généré par Nursera 🌱</p></body></html>");

    QTextDocument document;
    if (!logo.isNull())
        document.addResource(QTextDocument::ImageResource,
                             QUrl(QStringLiteral("logo")), logo);
    document.setHtml(html);

    const QString path = outputDir + QLatin1Char('/') + fileName
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
