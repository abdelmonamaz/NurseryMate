#include "controllers/report_controller.h"

#include "generators/report_generator.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QTextStream>
#include <QUrl>

#include <utility>

namespace nursera {
namespace {

QVariantList toVariant(const QList<ReportRow>& rows, const QLocale& locale)
{
    QVariantList out;
    for (const ReportRow& row : rows) {
        out.append(QVariantMap{
            {QStringLiteral("label"), row.label},
            {QStringLiteral("sub"), row.sub},
            {QStringLiteral("qty"), row.qty},
            {QStringLiteral("amount"), row.amount.toDisplayString(locale)},
            {QStringLiteral("amountMillimes"),
             static_cast<double>(row.amount.millimes())},
        });
    }
    return out;
}

QString csvCell(QString text)
{
    if (text.contains(QLatin1Char(';')) || text.contains(QLatin1Char('"'))
        || text.contains(QLatin1Char('\n'))) {
        text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + text + QLatin1Char('"');
    }
    return text;
}

} // namespace

ReportController::ReportController(IReportRepository& reports,
                                   ISettingsRepository& settings,
                                   QString exportDir, QObject* parent)
    : QObject(parent)
    , m_reports(reports)
    , m_settings(settings)
    , m_exportDir(std::move(exportDir))
{
    setThisMonth();
}

void ReportController::setFromDate(const QString& date)
{
    if (m_from == date)
        return;
    m_from = date;
    emit rangeChanged();
    refresh();
}

void ReportController::setToDate(const QString& date)
{
    if (m_to == date)
        return;
    m_to = date;
    emit rangeChanged();
    refresh();
}

void ReportController::setThisMonth()
{
    const QDate today = QDate::currentDate();
    m_from = QDate(today.year(), today.month(), 1).toString(Qt::ISODate);
    m_to = today.toString(Qt::ISODate);
    emit rangeChanged();
    refresh();
}

void ReportController::setLast30Days()
{
    const QDate today = QDate::currentDate();
    m_from = today.addDays(-29).toString(Qt::ISODate);
    m_to = today.toString(Qt::ISODate);
    emit rangeChanged();
    refresh();
}

void ReportController::refresh()
{
    const QLocale locale;

    if (const auto s = m_reports.salesSummary(m_from, m_to)) {
        m_summary = QVariantMap{
            {QStringLiteral("count"), s.value().saleCount},
            {QStringLiteral("total"), s.value().total.toDisplayString(locale)},
            {QStringLiteral("net"), s.value().net().toDisplayString(locale)},
            {QStringLiteral("credits"),
             s.value().creditTotal.toDisplayString(locale)},
            {QStringLiteral("hasCredits"),
             s.value().creditTotal.millimes() > 0},
            {QStringLiteral("average"),
             s.value().averageBasket().toDisplayString(locale)},
        };
    }
    if (const auto c = m_reports.salesByCategory(m_from, m_to))
        m_byCategory = toVariant(c.value(), locale);
    if (const auto p = m_reports.salesByPayment(m_from, m_to))
        m_byPayment = toVariant(p.value(), locale);
    if (const auto v = m_reports.stockValuation()) {
        m_stockVal = toVariant(v.value(), locale);
        Money total;
        for (const ReportRow& row : v.value())
            total = total + row.amount;
        m_stockValTotal = total.toDisplayString(locale);
    }
    if (const auto l = m_reports.productionLosses(m_from, m_to))
        m_losses = toVariant(l.value(), locale);

    // Rapport de marge (F10-05)
    m_marginRows.clear();
    if (const auto m = m_reports.margins(
            m_from, m_to, m_marginBy == QLatin1String("product"))) {
        Money totalRevenue, totalCost;
        for (const MarginRow& row : m.value()) {
            m_marginRows.append(QVariantMap{
                {QStringLiteral("label"), row.label},
                {QStringLiteral("qty"), row.qty},
                {QStringLiteral("revenue"), row.revenue.toDisplayString(locale)},
                {QStringLiteral("cost"), row.cost.toDisplayString(locale)},
                {QStringLiteral("margin"), row.margin().toDisplayString(locale)},
                {QStringLiteral("negative"), row.margin().millimes() < 0},
                {QStringLiteral("rate"),
                 QStringLiteral("%1 %").arg(row.marginPerMille() / 10.0, 0,
                                            'f', 1)},
            });
            totalRevenue = totalRevenue + row.revenue;
            totalCost = totalCost + row.cost;
        }
        const Money totalMargin = totalRevenue - totalCost;
        m_marginTotals = QVariantMap{
            {QStringLiteral("revenue"), totalRevenue.toDisplayString(locale)},
            {QStringLiteral("margin"), totalMargin.toDisplayString(locale)},
            {QStringLiteral("rate"),
             totalRevenue.millimes() > 0
                 ? QStringLiteral("%1 %").arg(
                       totalMargin.millimes() * 1000.0
                           / totalRevenue.millimes() / 10.0, 0, 'f', 1)
                 : QStringLiteral("—")},
        };
    }

    emit refreshed();
}

void ReportController::setMarginBy(const QString& by)
{
    if (m_marginBy == by)
        return;
    m_marginBy = by;
    emit marginByChanged();
    refresh();
}

bool ReportController::exportData(const QString& report, ExportData& data)
{
    const QLocale locale;
    if (report == QLatin1String("sales_category")) {
        if (const auto r = m_reports.salesByCategory(m_from, m_to))
            data.rows = r.value();
        data.title = tr("Ventes par catégorie");
        data.headers = {QStringLiteral("Catégorie"), QStringLiteral("Quantité"),
                        QStringLiteral("CA (DT)")};
        data.name = QStringLiteral("ventes-categories");
    } else if (report == QLatin1String("sales_payment")) {
        if (const auto r = m_reports.salesByPayment(m_from, m_to))
            data.rows = r.value();
        data.title = tr("Ventes par mode de paiement");
        data.headers = {QStringLiteral("Mode de paiement"), QStringLiteral("Ventes"),
                        QStringLiteral("Total (DT)")};
        data.name = QStringLiteral("ventes-paiement");
    } else if (report == QLatin1String("stock")) {
        if (const auto r = m_reports.stockValuation()) {
            data.rows = r.value();
            Money total;
            for (const ReportRow& row : r.value())
                total = total + row.amount;
            data.footer = tr("Total : %1").arg(total.toDisplayString(locale));
        }
        data.title = tr("Valorisation du stock (CMP)");
        data.headers = {QStringLiteral("Produit"), QStringLiteral("Conditionnement"),
                        QStringLiteral("Quantité"), QStringLiteral("Valeur CMP (DT)")};
        data.name = QStringLiteral("valorisation-stock");
        data.withSub = true;
        data.periodBound = false;
    } else if (report == QLatin1String("losses")) {
        if (const auto r = m_reports.productionLosses(m_from, m_to))
            data.rows = r.value();
        data.title = tr("Pertes de production");
        data.headers = {QStringLiteral("Motif"), QStringLiteral("Quantité perdue")};
        data.name = QStringLiteral("pertes-production");
        data.withValue = false;
    } else {
        return false;
    }
    return true;
}

// Table de marge partagée par les exports CSV et PDF (colonnes multiples).
namespace {

struct MarginTable
{
    QStringList headers;
    QList<QStringList> rows;
    QString footer;
};

} // namespace

static MarginTable buildMarginTable(const QList<MarginRow>& rows,
                                    const QLocale& locale)
{
    MarginTable table;
    table.headers = {QObject::tr("Libellé"), QObject::tr("Qté"),
                     QObject::tr("CA (DT)"), QObject::tr("Coût CMP (DT)"),
                     QObject::tr("Marge (DT)"), QObject::tr("Taux")};
    Money totalRevenue, totalCost;
    for (const MarginRow& row : rows) {
        table.rows.append({row.label, QString::number(row.qty),
                           row.revenue.toDisplayString(locale),
                           row.cost.toDisplayString(locale),
                           row.margin().toDisplayString(locale),
                           QStringLiteral("%1 %").arg(
                               row.marginPerMille() / 10.0, 0, 'f', 1)});
        totalRevenue = totalRevenue + row.revenue;
        totalCost = totalCost + row.cost;
    }
    table.footer = QObject::tr("Marge totale : %1")
                       .arg((totalRevenue - totalCost).toDisplayString(locale));
    return table;
}

QString ReportController::exportCsv(const QString& report)
{
    // Rapport de marge : colonnes spécifiques (CA / coût / marge / taux).
    if (report == QLatin1String("margin")) {
        const auto margins = m_reports.margins(
            m_from, m_to, m_marginBy == QLatin1String("product"));
        if (!margins)
            return {};
        if (!QDir().mkpath(m_exportDir)) {
            emit errorOccurred(tr("Dossier d'export inaccessible."));
            return {};
        }
        const QString path = m_exportDir + QStringLiteral("/marge-")
            + QDate::currentDate().toString(Qt::ISODate)
            + QStringLiteral(".csv");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit errorOccurred(tr("Écriture du CSV impossible."));
            return {};
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << QChar(0xFEFF);
        const MarginTable table = buildMarginTable(margins.value(), QLocale());
        out << table.headers.join(QLatin1Char(';')) << "\r\n";
        for (const QStringList& row : table.rows) {
            QStringList cells;
            for (const QString& cell : row)
                cells << csvCell(cell);
            out << cells.join(QLatin1Char(';')) << "\r\n";
        }
        file.close();
        return QUrl::fromLocalFile(path).toString();
    }

    ExportData data;
    if (!exportData(report, data))
        return {};

    if (!QDir().mkpath(m_exportDir)) {
        emit errorOccurred(tr("Dossier d'export inaccessible."));
        return {};
    }
    const QString path = m_exportDir + QLatin1Char('/') + data.name
        + QLatin1Char('-') + QDate::currentDate().toString(Qt::ISODate)
        + QStringLiteral(".csv");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(tr("Écriture du CSV impossible."));
        return {};
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QChar(0xFEFF); // BOM pour Excel
    out << data.headers.join(QLatin1Char(';')) << "\r\n";

    const QLocale locale;
    for (const ReportRow& row : data.rows) {
        QStringList cells;
        cells << csvCell(row.label);
        if (data.withSub)
            cells << csvCell(row.sub);
        cells << QString::number(row.qty);
        if (data.withValue)
            cells << csvCell(row.amount.toDisplayString(locale));
        out << cells.join(QLatin1Char(';')) << "\r\n";
    }
    file.close();
    return QUrl::fromLocalFile(path).toString();
}

QString ReportController::exportPdf(const QString& report)
{
    if (report == QLatin1String("margin")) {
        const auto margins = m_reports.margins(
            m_from, m_to, m_marginBy == QLatin1String("product"));
        if (!margins)
            return {};
        const MarginTable margin = buildMarginTable(margins.value(), QLocale());
        ReportGenerator::Table table;
        table.headers = margin.headers;
        table.rows = margin.rows;
        table.footer = margin.footer;
        const auto pdf = ReportGenerator::generatePdf(
            tr("Rapport de marge (CA − CMP)"),
            tr("Période : du %1 au %2").arg(m_from, m_to), {table},
            ReportGenerator::companyFrom(m_settings), m_exportDir,
            QStringLiteral("marge-")
                + QDate::currentDate().toString(Qt::ISODate));
        if (!pdf) {
            emit errorOccurred(tr("Génération du PDF impossible."));
            return {};
        }
        return QUrl::fromLocalFile(pdf.value()).toString();
    }

    ExportData data;
    if (!exportData(report, data))
        return {};

    const QLocale locale;
    ReportGenerator::Table table;
    table.title = data.title;
    table.headers = data.headers;
    table.footer = data.footer;
    for (const ReportRow& row : data.rows) {
        QStringList cells;
        cells << row.label;
        if (data.withSub)
            cells << row.sub;
        cells << QString::number(row.qty);
        if (data.withValue)
            cells << row.amount.toDisplayString(locale);
        table.rows.append(cells);
    }

    const QString period = data.periodBound
        ? tr("Période : du %1 au %2").arg(m_from, m_to)
        : tr("Photo du stock au %1")
              .arg(QDate::currentDate().toString(Qt::ISODate));

    const auto pdf = ReportGenerator::generatePdf(
        data.title, period, {table},
        ReportGenerator::companyFrom(m_settings), m_exportDir,
        data.name + QLatin1Char('-')
            + QDate::currentDate().toString(Qt::ISODate));
    if (!pdf) {
        emit errorOccurred(tr("Génération du PDF impossible."));
        return {};
    }
    return QUrl::fromLocalFile(pdf.value()).toString();
}

} // namespace nursera
