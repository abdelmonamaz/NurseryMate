#pragma once

#include "repositories/iquote_repository.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

class SqliteQuoteRepository : public IQuoteRepository
{
public:
    explicit SqliteQuoteRepository(QString connectionName);

    Result<Quote> create(const QuoteDraft& draft) override;
    Result<QList<QuoteRow>> list(int limit = 100) override;
    Result<QuoteDetails> details(int quoteId) override;
    Result<void> setStatus(int quoteId, QuoteStatus status) override;
    Result<void> setPdfPath(int quoteId, const QString& path) override;

private:
    QSqlDatabase db() const { return QSqlDatabase::database(m_connectionName); }

    QString m_connectionName;
};

} // namespace nursera
