#pragma once

#include "repositories/idashboard_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace nursera {

// Controller du tableau de bord (M10, F10-01) — lecture seule.
class DashboardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap kpis READ kpis NOTIFY refreshed)
    Q_PROPERTY(QVariantList topProducts READ topProducts NOTIFY refreshed)
    Q_PROPERTY(QVariantList lowStock READ lowStock NOTIFY refreshed)
    Q_PROPERTY(QVariantList salesDaily READ salesDaily NOTIFY refreshed)
    Q_PROPERTY(QVariantList salesMonthly READ salesMonthly NOTIFY refreshed)

public:
    explicit DashboardController(IDashboardRepository& dashboard,
                                 QObject* parent = nullptr);

    QVariantMap kpis() const { return m_kpis; }
    QVariantList topProducts() const { return m_topProducts; }
    QVariantList lowStock() const { return m_lowStock; }
    QVariantList salesDaily() const { return m_salesDaily; }
    QVariantList salesMonthly() const { return m_salesMonthly; }

    Q_INVOKABLE void refresh();

signals:
    void refreshed();
    void errorOccurred(const QString& message);

private:
    IDashboardRepository& m_dashboard;
    QVariantMap m_kpis;
    QVariantList m_topProducts;
    QVariantList m_lowStock;
    QVariantList m_salesDaily;
    QVariantList m_salesMonthly;
};

} // namespace nursera
