#pragma once

#include "qml_models/stock_list_model.h"
#include "repositories/ilocation_repository.h"
#include "repositories/istock_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace nursera {

// Controller de l'écran Stock (M03).
class StockController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(nursera::StockListModel* stock READ stock CONSTANT)
    Q_PROPERTY(QString searchTerm READ searchTerm WRITE setSearchTerm
                   NOTIFY searchTermChanged)
    Q_PROPERTY(int locationFilter READ locationFilter WRITE setLocationFilter
                   NOTIFY locationFilterChanged)

public:
    StockController(IStockRepository& stockRepository,
                    ILocationRepository& locations,
                    QObject* parent = nullptr);

    // Attribution des mouvements à l'utilisateur connecté (F01-04).
    void setUserIdProvider(std::function<int()> provider)
    {
        m_userIdProvider = std::move(provider);
    }

    StockListModel* stock() { return &m_model; }

    QString searchTerm() const { return m_searchTerm; }
    void setSearchTerm(const QString& term);

    int locationFilter() const { return m_locationFilter; }
    void setLocationFilter(int locationId);

    Q_INVOKABLE void refresh();

    // [{id, label}] pour les ComboBox d'emplacement.
    Q_INVOKABLE QVariantList locationOptions() const;

    // [{variantId, label, sku}] pour le sélecteur de variante.
    Q_INVOKABLE QVariantList searchVariants(const QString& term) const;

    // Historique des mouvements (F03-07). variantId <= 0 : tous.
    Q_INVOKABLE QVariantList history(int variantId) const;

    // data : kind ("in"|"out"|"transfer"), variantId, fromId, toId, qty,
    //        note, lossReason. Émet moveRecorded ou errorOccurred.
    Q_INVOKABLE bool recordMove(const QVariantMap& data);

signals:
    void searchTermChanged();
    void locationFilterChanged();
    void refreshed();
    void moveRecorded();
    void errorOccurred(const QString& message);

private:
    IStockRepository& m_stock;
    ILocationRepository& m_locations;
    StockListModel m_model;
    QString m_searchTerm;
    int m_locationFilter = 0;
    std::function<int()> m_userIdProvider;
};

} // namespace nursera
