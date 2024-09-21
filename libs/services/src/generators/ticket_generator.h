#pragma once

#include "common/result.h"
#include "models/sale.h"
#include "repositories/isettings_repository.h"

#include <QString>

namespace nursera {

// Ticket de caisse PDF (F04-06) — gabarit HTML rendu via QTextDocument
// vers QPdfWriter (doc 02 §7). Format A5, logo + coordonnées société.
class TicketGenerator
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

    // Lit les infos société depuis settings (clés company.*), avec les
    // valeurs Pépinière Idéale en repli.
    static CompanyInfo companyFrom(ISettingsRepository& settings);

    // Génère <outputDir>/<numéro>.pdf et retourne le chemin.
    static Result<QString> generatePdf(const SaleDetails& sale,
                                       const CompanyInfo& company,
                                       const QString& outputDir);
};

} // namespace nursera
