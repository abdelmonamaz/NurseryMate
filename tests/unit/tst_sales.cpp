#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace nursera;

class TestSales : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void moneyVatAndParse();
    void emptyCartRejected();
    void cashSaleFullFlow();
    void numbersHaveNoGaps();
    void discountApplied();
    void oversellGoesNegative();
    void journalAndTotals();
    void cancelRestoresStockAndTotals();
    void cancelValidation();
    void cashClosureGap();
    void creditNoteReversesSale();
    void partialCreditNote();
    void creditNoteValidation();

private:
    SaleDraft draftWith(int qty, Money discount = Money{}) const;

    QTemporaryDir m_dir;
    DatabaseManager* m_db = nullptr;
    SqliteStockRepository* m_stock = nullptr;
    SqliteSaleRepository* m_sales = nullptr;
    int m_variantId = 0;
    int m_vente = 0;
};

SaleDraft TestSales::draftWith(int qty, Money discount) const
{
    SaleLine line;
    line.variantId = m_variantId;
    line.label = QStringLiteral("Romarin — godet");
    line.qty = qty;
    line.unitPrice = Money::fromMillimes(3500);
    line.vatRatePercent = 19;

    SaleDraft draft;
    draft.lines = {line};
    draft.globalDiscount = discount;
    draft.stockLocationId = m_vente;
    return draft;
}

void TestSales::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_db = new DatabaseManager(m_dir.filePath(QStringLiteral("sales.db")),
                               QStringLiteral("tst_sales"));
    QVERIFY(m_db->open().isOk());
    QCOMPARE(m_db->schemaVersion(), 20);

    m_stock = new SqliteStockRepository(m_db->connectionName());
    m_sales = new SqliteSaleRepository(m_db->connectionName(), *m_stock);

    SqliteLocationRepository locations(m_db->connectionName());
    m_vente = locations.all().value().at(1).id; // Zone de vente

    SqliteProductRepository products(m_db->connectionName());
    Product romarin;
    romarin.nameFr = QStringLiteral("Romarin");
    Variant godet;
    godet.packaging = QStringLiteral("godet");
    godet.priceTtc = Money::fromMillimes(3500);
    godet.vatRatePercent = 19;
    const int productId = products.insertWithVariants(romarin, {godet}).value();
    m_variantId = products.variantsOf(productId).value().first().id;

    // 20 plants en zone de vente
    StockMove entry;
    entry.kind = MoveKind::In;
    entry.toLocationId = m_vente;
    entry.variantId = m_variantId;
    entry.qty = 20;
    QVERIFY(m_stock->recordMove(entry).isOk());
}

void TestSales::moneyVatAndParse()
{
    // TVA contenue dans le TTC : 11,900 à 19 % -> 1,900
    QCOMPARE(Money::fromMillimes(11900).vatFromTtc(19).millimes(), 1900);
    QCOMPARE(Money::fromMillimes(10000).vatFromTtc(0).millimes(), 0);

    // Parsing sans double (RT-01)
    QCOMPARE(Money::parseMillimes(QStringLiteral("12,500")), 12500);
    QCOMPARE(Money::parseMillimes(QStringLiteral("12.5")), 12500);
    QCOMPARE(Money::parseMillimes(QStringLiteral("50")), 50000);
    QCOMPARE(Money::parseMillimes(QStringLiteral("-3")), -1);
    QCOMPARE(Money::parseMillimes(QStringLiteral("abc")), -1);
}

void TestSales::emptyCartRejected()
{
    SaleDraft empty;
    empty.stockLocationId = m_vente;
    QVERIFY(!m_sales->record(empty).isOk());
}

void TestSales::cashSaleFullFlow()
{
    // 3 × 3,500 = 10,500 — espèces
    const auto sale = m_sales->record(draftWith(3));
    QVERIFY2(sale.isOk(),
             qPrintable(sale.isOk() ? QString() : sale.error().message));

    // Numéro conforme RG-04.b
    QCOMPARE(sale.value().number,
             QStringLiteral("T-%1-00001").arg(QDate::currentDate().year()));
    QCOMPARE(sale.value().total, Money::fromMillimes(10500));

    // Stock décrémenté par un mouvement refKind='sale' (RG-04.a)
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 17);

    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM stock_moves WHERE ref_kind = 'sale'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    // Paiement enregistré
    QVERIFY(query.exec(QStringLiteral(
        "SELECT method, amount FROM payments WHERE sale_id = %1")
                           .arg(sale.value().id)));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("cash"));
    QCOMPARE(query.value(1).toLongLong(), 10500);
}

void TestSales::numbersHaveNoGaps()
{
    const auto second = m_sales->record(draftWith(1));
    QVERIFY(second.isOk());
    QCOMPARE(second.value().number,
             QStringLiteral("T-%1-00002").arg(QDate::currentDate().year()));

    // Une vente refusée (panier vide) ne consomme pas de numéro
    SaleDraft empty;
    empty.stockLocationId = m_vente;
    QVERIFY(!m_sales->record(empty).isOk());

    const auto third = m_sales->record(draftWith(1));
    QVERIFY(third.isOk());
    QCOMPARE(third.value().number,
             QStringLiteral("T-%1-00003").arg(QDate::currentDate().year()));
}

void TestSales::discountApplied()
{
    // 2 × 3,500 = 7,000 − remise 1,000 = 6,000
    const auto sale = m_sales->record(draftWith(2, Money::fromMillimes(1000)));
    QVERIFY(sale.isOk());
    QCOMPARE(sale.value().total, Money::fromMillimes(6000));

    // Remise > total refusée
    QVERIFY(!m_sales->record(draftWith(1, Money::fromMillimes(99000))).isOk());
}

void TestSales::oversellGoesNegative()
{
    // Stock restant : 20 - 3 - 1 - 1 - 2 = 13 ; vente de 50 -> -37 (RG-03.b)
    const auto sale = m_sales->record(draftWith(50));
    QVERIFY(sale.isOk());
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, -37);
}

void TestSales::journalAndTotals()
{
    const auto journal = m_sales->todayJournal();
    QVERIFY(journal.isOk());
    QCOMPARE(journal.value().size(), 5);
    // Plus récente d'abord
    QVERIFY(journal.value().first().number
            > journal.value().last().number);

    const auto totals = m_sales->todayTotals();
    QVERIFY(totals.isOk());
    QCOMPARE(totals.value().saleCount, 5);
    // 10,500 + 3,500 + 3,500 + 6,000 + 175,000 = 198,500
    QCOMPARE(totals.value().total, Money::fromMillimes(198500));
    QCOMPARE(totals.value().cash, Money::fromMillimes(198500));
    QCOMPARE(totals.value().cheque, Money::fromMillimes(0));
}

void TestSales::cancelRestoresStockAndTotals()
{
    // Annulation de la grosse vente (50 × 3,500 = 175,000) — F04-08
    const int bigSaleId = m_sales->todayJournal().value().first().id;
    QVERIFY(m_sales->cancel(bigSaleId, QStringLiteral("erreur de saisie"), 0).isOk());

    // Stock restitué : -37 + 50 = 13
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 13);

    // Contre-mouvement tracé
    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT count(*) FROM stock_moves WHERE ref_kind = 'sale_cancel'")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);

    // Totaux du jour recalculés sans la vente annulée : 198,500 - 175,000
    const auto totals = m_sales->todayTotals();
    QCOMPARE(totals.value().saleCount, 4);
    QCOMPARE(totals.value().total, Money::fromMillimes(23500));
    QCOMPARE(totals.value().cash, Money::fromMillimes(23500));

    // Le journal la garde, marquée annulée (traçabilité F01-06)
    QCOMPARE(m_sales->todayJournal().value().size(), 5);
    QCOMPARE(m_sales->todayJournal().value().first().status,
             QStringLiteral("cancelled"));
}

void TestSales::cancelValidation()
{
    const int cancelledId = m_sales->todayJournal().value().first().id;

    // Motif obligatoire
    QVERIFY(!m_sales->cancel(cancelledId, QStringLiteral("  "), 0).isOk());
    // Double annulation refusée
    const auto twice = m_sales->cancel(cancelledId, QStringLiteral("re"), 0);
    QVERIFY(!twice.isOk());
    QCOMPARE(twice.error().code, QStringLiteral("sale.cancelled"));
    // Vente inexistante
    QVERIFY(!m_sales->cancel(99999, QStringLiteral("x"), 0).isOk());

    // Le stock n'a pas bougé
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty, 13);
}

void TestSales::cashClosureGap()
{
    // Espèces théoriques du jour = total cash des ventes non annulées.
    // Après annulation de la grosse vente, cash restant = 23,500.
    const auto expected = m_sales->todayTotals();
    QCOMPARE(expected.value().cash, Money::fromMillimes(23500));

    // Compté 23,000 -> écart -500 journalisé (F04-09)
    const auto gap = m_sales->recordCashClosure(Money::fromMillimes(23000), 0);
    QVERIFY2(gap.isOk(), qPrintable(gap.isOk() ? QString() : gap.error().message));
    QCOMPARE(gap.value(), Money::fromMillimes(-500));

    QSqlQuery query(m_db->database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT expected_cash, counted_cash, gap FROM cash_closures")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toLongLong(), 23500);
    QCOMPARE(query.value(1).toLongLong(), 23000);
    QCOMPARE(query.value(2).toLongLong(), -500);
}

void TestSales::creditNoteReversesSale()
{
    // Vente espèces 2 × 3,500 = 7,000, puis avoir avec retour en stock.
    const auto sale = m_sales->record(draftWith(2));
    QVERIFY(sale.isOk());
    const int stockBefore =
        m_stock->levelsOf(m_variantId).value().first().qty;

    CreditNoteDraft draft;
    draft.saleId = sale.value().id;
    draft.reason = QStringLiteral("saisie erronée détectée à la vérification");
    draft.restock = true;
    draft.refundMethod = QStringLiteral("cash");
    draft.stockLocationId = m_vente;
    const auto note = m_sales->createCreditNote(draft);
    QVERIFY2(note.isOk(),
             qPrintable(note.isOk() ? QString() : note.error().message));
    const int year = QDate::currentDate().year();
    QCOMPARE(note.value().number, QStringLiteral("AV-%1-001").arg(year));
    QCOMPARE(note.value().total, Money::fromMillimes(7000));

    // Stock restitué par des ré-entrées ref_kind='credit_note'.
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty,
             stockBefore + 2);
    QSqlQuery moves(m_db->database());
    QVERIFY(moves.exec(QStringLiteral(
        "SELECT COALESCE(SUM(qty), 0) FROM stock_moves "
        "WHERE ref_kind = 'credit_note'")));
    QVERIFY(moves.next());
    QCOMPARE(moves.value(0).toInt(), 2);

    // Le remboursement espèces du jour sort du théorique de caisse :
    // théorique = 23,500 + 7,000 (vente) − 7,000 (avoir) = 23,500.
    const auto gap = m_sales->recordCashClosure(Money::fromMillimes(23500), 0);
    QVERIFY(gap.isOk());
    QCOMPARE(gap.value(), Money::fromMillimes(0));

    // Le journal du jour porte le numéro d'avoir sur la vente.
    bool found = false;
    for (const SaleJournalRow& row : m_sales->todayJournal().value())
        if (row.id == sale.value().id) {
            QCOMPARE(row.creditNoteNumber, note.value().number);
            found = true;
        }
    QVERIFY(found);

    // Vente entièrement remboursée : plus rien à rembourser.
    const auto again = m_sales->createCreditNote(draft);
    QVERIFY(!again.isOk());
    QCOMPARE(again.error().code, QStringLiteral("credit.nothing"));

    // Détail de l'avoir (PDF) : entête + lignes remboursées.
    const auto details = m_sales->creditNoteDetails(sale.value().id);
    QVERIFY2(details.isOk(),
             qPrintable(details.isOk() ? QString() : details.error().message));
    QCOMPARE(details.value().number, note.value().number);
    QCOMPARE(details.value().total, Money::fromMillimes(7000));
    QCOMPARE(details.value().refundMethod, QStringLiteral("cash"));
    QVERIFY(details.value().restock);
    QCOMPARE(details.value().reason, draft.reason);
    QCOMPARE(details.value().sale.number, sale.value().number);
    QCOMPARE(details.value().lines.size(), 1);
    QCOMPARE(details.value().lines.first().qty, 2);
}

void TestSales::partialCreditNote()
{
    // Vente 4 × 3,500 = 14,000 − remise 1,400 = 12,600 (facteur 0,9).
    const auto sale =
        m_sales->record(draftWith(4, Money::fromMillimes(1400)));
    QVERIFY(sale.isOk());
    const int stockBefore =
        m_stock->levelsOf(m_variantId).value().first().qty;

    const auto lines = m_sales->refundableLines(sale.value().id);
    QVERIFY(lines.isOk());
    QCOMPARE(lines.value().size(), 1);
    const int saleLineId = lines.value().first().saleLineId;

    // 1er avoir partiel : 1 sur 4 -> 3,500 × 0,9 = 3,150 (remise au prorata),
    // et seul 1 plant revient en stock.
    CreditNoteDraft partial;
    partial.saleId = sale.value().id;
    partial.reason = QStringLiteral("un plant abîmé rendu");
    partial.restock = true;
    partial.stockLocationId = m_vente;
    partial.lines = {{saleLineId, 1}};
    const auto first = m_sales->createCreditNote(partial);
    QVERIFY2(first.isOk(),
             qPrintable(first.isOk() ? QString() : first.error().message));
    QCOMPARE(first.value().total, Money::fromMillimes(3150));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty,
             stockBefore + 1);

    // Sur-remboursement refusé : il ne reste que 3 remboursables.
    CreditNoteDraft tooMuch = partial;
    tooMuch.lines = {{saleLineId, 4}};
    const auto refused = m_sales->createCreditNote(tooMuch);
    QVERIFY(!refused.isOk());
    QCOMPARE(refused.error().code, QStringLiteral("credit.qty"));

    // 2e avoir : le reste (3) — réconciliation exacte : cumul des avoirs
    // == total de la vente, au millime (12,600 − 3,150 = 9,450).
    CreditNoteDraft rest = partial;
    rest.lines.clear(); // vide = tout le restant
    const auto second = m_sales->createCreditNote(rest);
    QVERIFY(second.isOk());
    QCOMPARE(second.value().total, Money::fromMillimes(9450));
    QCOMPARE(m_stock->levelsOf(m_variantId).value().first().qty,
             stockBefore + 4);

    // Le journal porte le cumul et le compte d'avoirs.
    for (const SaleJournalRow& row : m_sales->todayJournal().value())
        if (row.id == sale.value().id) {
            QCOMPARE(row.creditNoteCount, 2);
            QCOMPARE(row.refundedTotal, Money::fromMillimes(12600));
            QCOMPARE(row.creditNoteNumber, second.value().number);
        }
}

void TestSales::creditNoteValidation()
{
    const auto sale = m_sales->record(draftWith(1));
    QVERIFY(sale.isOk());

    // Pas d'avoir sur cette vente : le détail est refusé.
    const auto noNote = m_sales->creditNoteDetails(sale.value().id);
    QVERIFY(!noNote.isOk());
    QCOMPARE(noNote.error().code, QStringLiteral("credit.notFound"));

    // Motif obligatoire.
    CreditNoteDraft noReason;
    noReason.saleId = sale.value().id;
    noReason.stockLocationId = m_vente;
    QVERIFY(!m_sales->createCreditNote(noReason).isOk());

    // Emplacement obligatoire si retour en stock.
    CreditNoteDraft noLocation;
    noLocation.saleId = sale.value().id;
    noLocation.reason = QStringLiteral("test");
    noLocation.restock = true;
    QVERIFY(!m_sales->createCreditNote(noLocation).isOk());

    // Vente annulée : pas d'avoir.
    QVERIFY(m_sales->cancel(sale.value().id,
                            QStringLiteral("annulée d'abord"), 0).isOk());
    CreditNoteDraft onCancelled;
    onCancelled.saleId = sale.value().id;
    onCancelled.reason = QStringLiteral("test");
    onCancelled.restock = false;
    const auto refused = m_sales->createCreditNote(onCancelled);
    QVERIFY(!refused.isOk());
    QCOMPARE(refused.error().code, QStringLiteral("credit.cancelled"));
}

QTEST_GUILESS_MAIN(TestSales)
#include "tst_sales.moc"
