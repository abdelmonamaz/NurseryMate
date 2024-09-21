#pragma once

#include "repositories/iquote_repository.h"
#include "repositories/isettings_repository.h"
#include "repositories/istock_repository.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de l'écran Devis (M05).
class QuoteController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList quotes READ quotes NOTIFY refreshed)

public:
    QuoteController(IQuoteRepository& quotes,
                    IStockRepository& stock,
                    ISettingsRepository& settings,
                    QString outputDir,
                    QObject* parent = nullptr);

    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    QVariantList quotes() const { return m_quotes; }

    Q_INVOKABLE void refresh();

    // Recherche de variante pour ajouter une ligne produit.
    Q_INVOKABLE QVariantList searchVariants(const QString& term) const;
    Q_INVOKABLE QVariantList searchCustomers(const QString& term) const;

    // data : customerId, customerName, discount ("3,500"), note,
    //        lines: [{variantId (0 = libre), label, qty, price}].
    Q_INVOKABLE bool createQuote(const QVariantMap& data);

    // Génère/ouvre le PDF ; retourne l'url file://.
    Q_INVOKABLE QString pdfFor(int quoteId);
    // Change le statut : "sent" | "accepted" | "refused".
    Q_INVOKABLE bool setStatus(int quoteId, const QString& status);
    // Détail d'un devis pour l'écran (lignes incluses) : { number,
    // customerName, status, createdAt, validUntil, note, subtotal, discount,
    // total, lines: [{label, qty, unitPrice, lineTotal}] }.
    Q_INVOKABLE QVariantMap quoteDetail(int quoteId);

signals:
    void refreshed();
    void quoteCreated(int quoteId);
    void errorOccurred(const QString& message);

private:
    IQuoteRepository& m_repo;
    IStockRepository& m_stock;
    ISettingsRepository& m_settings;
    QString m_outputDir;
    QVariantList m_quotes;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
