#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_quote_repository.h"

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestQuotes : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void createNumberedQuote();
    void detailsAndLines();
    void statusTransition();
    void autoExpiration();
    void doesNotTouchStock();

private:
    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteQuoteRepository* m_quotes = nullptr;
    int m_quoteId = 0;
};

void TestQuotes::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("quotes.db")),
                               QStringLiteral("tst_quotes"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);
    m_quotes = new SqliteQuoteRepository(m_db->connectionName());
}

void TestQuotes::createNumberedQuote()
{
    QuoteLine product;
    product.label = QStringLiteral("Olivier — pot21");
    product.qty = 10;
    product.unitPrice = Money::fromMillimes(28000);
    QuoteLine service; // ligne libre (prestation)
    service.label = QStringLiteral("Plantation sur site");
    service.qty = 1;
    service.unitPrice = Money::fromMillimes(150000);

    QuoteDraft draft;
    draft.customerName = QStringLiteral("Hôtel Syphax");
    draft.discount = Money::fromMillimes(30000);
    draft.lines = {product, service};

    const auto quote = m_quotes->create(draft);
    QVERIFY2(quote.isOk(),
             qPrintable(quote.isOk() ? QString() : quote.error().message));
    QVERIFY(quote.value().number.startsWith(QStringLiteral("D-")));
    QVERIFY(quote.value().number.endsWith(QStringLiteral("00001")));
    // 280,000 + 150,000 − 30,000 = 400,000
    QCOMPARE(quote.value().total, Money::fromMillimes(400000));
    m_quoteId = quote.value().id;

    // Devis vide refusé
    QuoteDraft empty;
    QVERIFY(!m_quotes->create(empty).isOk());
}

void TestQuotes::detailsAndLines()
{
    const auto details = m_quotes->details(m_quoteId);
    QVERIFY(details.isOk());
    QCOMPARE(details.value().customerName, QStringLiteral("Hôtel Syphax"));
    QCOMPARE(details.value().status, QuoteStatus::Draft);
    QCOMPARE(details.value().lines.size(), 2);
    QCOMPARE(details.value().subtotal, Money::fromMillimes(430000));
    QCOMPARE(details.value().discount, Money::fromMillimes(30000));
    QCOMPARE(details.value().total, Money::fromMillimes(400000));
    // Validité par défaut à 30 jours renseignée
    QVERIFY(!details.value().validUntil.isEmpty());

    // Liste : le devis apparaît
    QCOMPARE(m_quotes->list().value().size(), 1);
    QCOMPARE(m_quotes->list().value().first().number, details.value().number);
}

void TestQuotes::statusTransition()
{
    QVERIFY(m_quotes->setStatus(m_quoteId, QuoteStatus::Sent).isOk());
    QCOMPARE(m_quotes->details(m_quoteId).value().status, QuoteStatus::Sent);
    QVERIFY(m_quotes->setStatus(m_quoteId, QuoteStatus::Accepted).isOk());
    QCOMPARE(m_quotes->details(m_quoteId).value().status, QuoteStatus::Accepted);
}

void TestQuotes::autoExpiration()
{
    // Devis envoyé dont la validité est passée -> 'expired' au listage.
    QuoteLine line;
    line.label = QStringLiteral("Prestation test");
    line.qty = 1;
    line.unitPrice = Money::fromMillimes(10000);
    QuoteDraft draft;
    draft.customerName = QStringLiteral("Prospect");
    draft.validUntil = QStringLiteral("2020-01-01"); // dépassée
    draft.lines = {line};
    const int expiredId = m_quotes->create(draft).value().id;

    // Un devis ACCEPTÉ à validité passée reste figé (m_quoteId, test
    // précédent — sa validité est future, on vérifie surtout le non-impact).
    QVERIFY(m_quotes->list().isOk()); // déclenche l'expiration

    QCOMPARE(m_quotes->details(expiredId).value().status, QuoteStatus::Expired);
    QCOMPARE(m_quotes->details(m_quoteId).value().status, QuoteStatus::Accepted);

    // Réouverture (fausse manip) : retour à Envoyé + validité prolongée,
    // sinon il re-expirerait au prochain listage.
    QVERIFY(m_quotes->setStatus(expiredId, QuoteStatus::Sent).isOk());
    QVERIFY(m_quotes->list().isOk());
    QCOMPARE(m_quotes->details(expiredId).value().status, QuoteStatus::Sent);
    QVERIFY(m_quotes->details(expiredId).value().validUntil
             >= QDate::currentDate().toString(Qt::ISODate));
}

void TestQuotes::doesNotTouchStock()
{
    // RG-05.b : un devis ne génère aucun mouvement de stock
    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral("SELECT count(*) FROM stock_moves")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
}

QTEST_GUILESS_MAIN(TestQuotes)
#include "tst_quotes.moc"
