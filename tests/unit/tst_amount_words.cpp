#include "common/amount_to_words.h"

#include <QtTest>

using namespace nursera;

class TestAmountWords : public QObject
{
    Q_OBJECT

private slots:
    void smallNumbers();
    void tensEdgeCases();
    void hundredsPlural();
    void thousandsAndBeyond();
    void amounts();
};

void TestAmountWords::smallNumbers()
{
    QCOMPARE(AmountToWords::frenchNumber(0), QStringLiteral("zéro"));
    QCOMPARE(AmountToWords::frenchNumber(1), QStringLiteral("un"));
    QCOMPARE(AmountToWords::frenchNumber(16), QStringLiteral("seize"));
    QCOMPARE(AmountToWords::frenchNumber(17), QStringLiteral("dix-sept"));
    QCOMPARE(AmountToWords::frenchNumber(19), QStringLiteral("dix-neuf"));
}

void TestAmountWords::tensEdgeCases()
{
    QCOMPARE(AmountToWords::frenchNumber(20), QStringLiteral("vingt"));
    QCOMPARE(AmountToWords::frenchNumber(21), QStringLiteral("vingt et un"));
    QCOMPARE(AmountToWords::frenchNumber(22), QStringLiteral("vingt-deux"));
    QCOMPARE(AmountToWords::frenchNumber(31), QStringLiteral("trente et un"));
    QCOMPARE(AmountToWords::frenchNumber(70), QStringLiteral("soixante-dix"));
    QCOMPARE(AmountToWords::frenchNumber(71), QStringLiteral("soixante et onze"));
    QCOMPARE(AmountToWords::frenchNumber(72), QStringLiteral("soixante-douze"));
    QCOMPARE(AmountToWords::frenchNumber(77), QStringLiteral("soixante-dix-sept"));
    QCOMPARE(AmountToWords::frenchNumber(80), QStringLiteral("quatre-vingts"));
    QCOMPARE(AmountToWords::frenchNumber(81), QStringLiteral("quatre-vingt-un"));
    QCOMPARE(AmountToWords::frenchNumber(90), QStringLiteral("quatre-vingt-dix"));
    QCOMPARE(AmountToWords::frenchNumber(91), QStringLiteral("quatre-vingt-onze"));
    QCOMPARE(AmountToWords::frenchNumber(99), QStringLiteral("quatre-vingt-dix-neuf"));
}

void TestAmountWords::hundredsPlural()
{
    QCOMPARE(AmountToWords::frenchNumber(100), QStringLiteral("cent"));
    QCOMPARE(AmountToWords::frenchNumber(101), QStringLiteral("cent un"));
    QCOMPARE(AmountToWords::frenchNumber(180), QStringLiteral("cent quatre-vingts"));
    QCOMPARE(AmountToWords::frenchNumber(200), QStringLiteral("deux cents"));
    QCOMPARE(AmountToWords::frenchNumber(201), QStringLiteral("deux cent un"));
    QCOMPARE(AmountToWords::frenchNumber(299),
             QStringLiteral("deux cent quatre-vingt-dix-neuf"));
    QCOMPARE(AmountToWords::frenchNumber(999),
             QStringLiteral("neuf cent quatre-vingt-dix-neuf"));
}

void TestAmountWords::thousandsAndBeyond()
{
    QCOMPARE(AmountToWords::frenchNumber(1000), QStringLiteral("mille"));
    QCOMPARE(AmountToWords::frenchNumber(1001), QStringLiteral("mille un"));
    QCOMPARE(AmountToWords::frenchNumber(2000), QStringLiteral("deux mille"));
    // "mille" invariable, "quatre-vingt"/"cent" sans s devant mille
    QCOMPARE(AmountToWords::frenchNumber(80000), QStringLiteral("quatre-vingt mille"));
    QCOMPARE(AmountToWords::frenchNumber(200000), QStringLiteral("deux cent mille"));
    QCOMPARE(AmountToWords::frenchNumber(1234),
             QStringLiteral("mille deux cent trente-quatre"));
    QCOMPARE(AmountToWords::frenchNumber(1000000), QStringLiteral("un million"));
    QCOMPARE(AmountToWords::frenchNumber(2000000), QStringLiteral("deux millions"));
    QCOMPARE(AmountToWords::frenchNumber(1000000000), QStringLiteral("un milliard"));
}

void TestAmountWords::amounts()
{
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(0)),
             QStringLiteral("Zéro dinar"));
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(1000)),
             QStringLiteral("Un dinar"));
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(2000)),
             QStringLiteral("Deux dinars"));
    // 125,500
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(125500)),
             QStringLiteral("Cent vingt-cinq dinars et cinq cents millimes"));
    // Millimes seuls
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(500)),
             QStringLiteral("Cinq cents millimes"));
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(1)),
             QStringLiteral("Un millime"));
    // 43,000 (exemple caisse)
    QCOMPARE(AmountToWords::frenchAmount(Money::fromMillimes(43000)),
             QStringLiteral("Quarante-trois dinars"));
}

QTEST_APPLESS_MAIN(TestAmountWords)
#include "tst_amount_words.moc"
