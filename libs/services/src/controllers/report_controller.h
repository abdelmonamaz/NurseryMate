#pragma once

#include "repositories/ireport_repository.h"
#include "repositories/isettings_repository.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace nursera {

// Controller de l'écran Rapports (M10). Plage de dates + export CSV.
class ReportController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fromDate READ fromDate WRITE setFromDate NOTIFY rangeChanged)
    Q_PROPERTY(QString toDate READ toDate WRITE setToDate NOTIFY rangeChanged)
    Q_PROPERTY(QVariantMap salesSummary READ salesSummary NOTIFY refreshed)
    Q_PROPERTY(QVariantList salesByCategory READ salesByCategory NOTIFY refreshed)
    Q_PROPERTY(QVariantList salesByPayment READ salesByPayment NOTIFY refreshed)
    Q_PROPERTY(QVariantList stockValuation READ stockValuation NOTIFY refreshed)
    Q_PROPERTY(QString stockValuationTotal READ stockValuationTotal NOTIFY refreshed)
    Q_PROPERTY(QVariantList productionLosses READ productionLosses NOTIFY refreshed)
    Q_PROPERTY(QVariantList marginRows READ marginRows NOTIFY refreshed)
    Q_PROPERTY(QVariantMap marginTotals READ marginTotals NOTIFY refreshed)
    // "category" | "product" — groupement du rapport de marge (F10-05)
    Q_PROPERTY(QString marginBy READ marginBy WRITE setMarginBy
                   NOTIFY marginByChanged)

public:
    ReportController(IReportRepository& reports, ISettingsRepository& settings,
                     QString exportDir, QObject* parent = nullptr);

    QString fromDate() const { return m_from; }
    void setFromDate(const QString& date);
    QString toDate() const { return m_to; }
    void setToDate(const QString& date);

    QVariantMap salesSummary() const { return m_summary; }
    QVariantList salesByCategory() const { return m_byCategory; }
    QVariantList salesByPayment() const { return m_byPayment; }
    QVariantList stockValuation() const { return m_stockVal; }
    QString stockValuationTotal() const { return m_stockValTotal; }
    QVariantList productionLosses() const { return m_losses; }
    QVariantList marginRows() const { return m_marginRows; }
    QVariantMap marginTotals() const { return m_marginTotals; }
    QString marginBy() const { return m_marginBy; }
    void setMarginBy(const QString& by);

    Q_INVOKABLE void refresh();
    // Raccourcis de période.
    Q_INVOKABLE void setThisMonth();
    Q_INVOKABLE void setLast30Days();

    // Exporte un rapport en CSV (UTF-8 BOM, séparateur ';' pour Excel FR).
    // report : "sales_category" | "sales_payment" | "stock" | "losses".
    // Retourne l'url file:// du fichier, ou "".
    Q_INVOKABLE QString exportCsv(const QString& report);

    // Exporte le même rapport en PDF A4 mis en page (F10-06) — mêmes clés.
    Q_INVOKABLE QString exportPdf(const QString& report);

signals:
    void rangeChanged();
    void marginByChanged();
    void refreshed();
    void errorOccurred(const QString& message);

private:
    // Données communes aux exports CSV et PDF d'un rapport donné.
    struct ExportData
    {
        QString title;       // "Ventes par catégorie"
        QString name;        // base du nom de fichier
        QStringList headers;
        QList<ReportRow> rows;
        bool withSub = false;   // colonne conditionnement (stock)
        bool withValue = true;  // colonne montant
        QString footer;         // total (stock)
        bool periodBound = true; // false = photo courante (stock)
    };
    bool exportData(const QString& report, ExportData& data);

    IReportRepository& m_reports;
    ISettingsRepository& m_settings;
    QString m_exportDir;
    QString m_from;
    QString m_to;
    QVariantMap m_summary;
    QVariantList m_byCategory;
    QVariantList m_byPayment;
    QVariantList m_stockVal;
    QString m_stockValTotal;
    QVariantList m_losses;
    QVariantList m_marginRows;
    QVariantMap m_marginTotals;
    QString m_marginBy = QStringLiteral("category");
};

} // namespace nursera
