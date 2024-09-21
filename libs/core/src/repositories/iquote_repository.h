#pragma once

#include "common/result.h"
#include "models/quote.h"

#include <QList>

namespace nursera {

class IQuoteRepository
{
public:
    virtual ~IQuoteRepository() = default;

    // Crée un devis (numéro D-AAAA-NNNNN sans trou). Ne touche pas le
    // stock (RG-05.b).
    virtual Result<Quote> create(const QuoteDraft& draft) = 0;

    virtual Result<QList<QuoteRow>> list(int limit = 100) = 0;
    virtual Result<QuoteDetails> details(int quoteId) = 0;
    virtual Result<void> setStatus(int quoteId, QuoteStatus status) = 0;
    virtual Result<void> setPdfPath(int quoteId, const QString& path) = 0;
};

} // namespace nursera
