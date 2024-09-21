#pragma once

#include "common/result.h"
#include "repositories/isettings_repository.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace nursera {

// Rapport PDF A4 (F10-06) : en-tête société, titre + période, un ou
// plusieurs tableaux (1re colonne à gauche, les autres à droite), total.
class ReportGenerator
{
public:
    struct CompanyInfo
    {
        QString name;
        QString tagline;
        QString address;
        QString phone;
        QString taxId;
    };

    struct Table
    {
        QString title;           // ex. "Ventes par catégorie"
        QStringList headers;     // ex. {"Catégorie", "Qté", "CA"}
        QList<QStringList> rows; // cellules déjà formatées (montants inclus)
        QString footer;          // ligne de total optionnelle
    };

    static CompanyInfo companyFrom(ISettingsRepository& settings);

    // fileName sans extension (ex. "ventes-categories-2026-07-15").
    static Result<QString> generatePdf(const QString& title,
                                       const QString& periodText,
                                       const QList<Table>& tables,
                                       const CompanyInfo& company,
                                       const QString& outputDir,
                                       const QString& fileName);
};

} // namespace nursera
