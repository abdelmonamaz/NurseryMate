#pragma once

#include "repositories/iinvoice_repository.h"
#include "repositories/isettings_repository.h"

#include <QObject>
#include <QString>

#include <functional>

namespace nursera {

// Émission et rendu des factures (M05) — piloté depuis le journal de caisse.
class InvoiceController : public QObject
{
    Q_OBJECT

public:
    InvoiceController(IInvoiceRepository& invoices,
                      ISettingsRepository& settings,
                      QString outputDir,
                      QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    // Émet (si besoin) la facture de la vente, génère le PDF et retourne
    // son url file:// (ou "" en cas d'erreur). Timbre fiscal lu des
    // paramètres (finance.stamp_duty).
    Q_INVOKABLE QString invoiceForSale(int saleId);

signals:
    void errorOccurred(const QString& message);

private:
    IInvoiceRepository& m_invoices;
    ISettingsRepository& m_settings;
    QString m_outputDir;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
