#pragma once

#include "repositories/ireport_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteReportRepository : public IReportRepository
{
public:
    explicit SqliteReportRepository(QString connectionName);

    Result<SalesSummary> salesSummary(const QString& from,
                                      const QString& to) override;
    Result<QList<ReportRow>> salesByCategory(const QString& from,
                                             const QString& to) override;
    Result<QList<ReportRow>> salesByPayment(const QString& from,
                                            const QString& to) override;
    Result<QList<ReportRow>> stockValuation() override;
    Result<QList<ReportRow>> productionLosses(const QString& from,
                                              const QString& to) override;
    Result<QList<MarginRow>> margins(const QString& from, const QString& to,
                                     bool byProduct) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
