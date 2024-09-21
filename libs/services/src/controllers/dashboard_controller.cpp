#include "controllers/dashboard_controller.h"

#include <QLocale>

namespace nursera {

DashboardController::DashboardController(IDashboardRepository& dashboard,
                                         QObject* parent)
    : QObject(parent)
    , m_dashboard(dashboard)
{
}

void DashboardController::refresh()
{
    const QLocale locale;

    const auto kpis = m_dashboard.kpis();
    if (!kpis) {
        emit errorOccurred(kpis.error().message);
        return;
    }
    m_kpis = QVariantMap{
        {QStringLiteral("todayCount"), kpis.value().todaySaleCount},
        {QStringLiteral("todayTotal"), kpis.value().todayTotal.toDisplayString(locale)},
        {QStringLiteral("weekTotal"), kpis.value().weekTotal.toDisplayString(locale)},
        {QStringLiteral("monthTotal"), kpis.value().monthTotal.toDisplayString(locale)},
        {QStringLiteral("averageBasket"),
         kpis.value().averageBasket().toDisplayString(locale)},
    };

    m_topProducts.clear();
    if (const auto top = m_dashboard.topProducts()) {
        for (const TopProductRow& row : top.value()) {
            m_topProducts.append(QVariantMap{
                {QStringLiteral("label"), row.label},
                {QStringLiteral("qtySold"), row.qtySold},
                {QStringLiteral("revenue"), row.revenue.toDisplayString(locale)},
            });
        }
    }

    m_lowStock.clear();
    if (const auto low = m_dashboard.lowStock()) {
        for (const LowStockRow& row : low.value()) {
            m_lowStock.append(QVariantMap{
                {QStringLiteral("label"),
                 QStringLiteral("%1 — %2").arg(row.productFr, row.packaging)},
                {QStringLiteral("qty"), row.qty},
                {QStringLiteral("threshold"), row.alertThreshold},
                {QStringLiteral("negative"), row.isNegative()},
            });
        }
    }

    // Séries pour la courbe (14 j) et le diagramme (6 mois)
    const auto fillTrend = [&locale](QVariantList& out, auto&& result) {
        out.clear();
        if (!result)
            return;
        for (const TrendPoint& point : result.value()) {
            out.append(QVariantMap{
                {QStringLiteral("label"), point.label},
                {QStringLiteral("fullLabel"), point.fullLabel},
                {QStringLiteral("value"), double(point.total.millimes()) / 1000.0},
                {QStringLiteral("display"), point.total.toDisplayString(locale)},
            });
        }
    };
    fillTrend(m_salesDaily, m_dashboard.salesDaily());
    fillTrend(m_salesMonthly, m_dashboard.salesMonthly());

    emit refreshed();
}

} // namespace nursera
