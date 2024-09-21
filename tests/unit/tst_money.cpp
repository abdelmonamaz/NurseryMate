#include "common/money.h"

#include <QtTest>

using nursera::Money;

class TestMoney : public QObject
{
    Q_OBJECT

private slots:
    void arithmetic();
    void percentHalfUpRounding();
    void displayFrench();
    void displayArabic();
    void displayNegative();
};

void TestMoney::arithmetic()
{
    const Money price = Money::fromTnd(12, 500);
    QCOMPARE(price.millimes(), 12500);
    QCOMPARE((price + Money::fromMillimes(500)).millimes(), 13000);
    QCOMPARE((price - Money::fromTnd(2)).millimes(), 10500);
    QCOMPARE((price * 3).millimes(), 37500);
    QCOMPARE((-price).millimes(), -12500);
    QVERIFY(Money::fromTnd(1) > Money::fromMillimes(999));
}

void TestMoney::percentHalfUpRounding()
{
    // 10 % de 12,345 DT = 1,2345 → demi-supérieur : 1,235 (RT-01)
    QCOMPARE(Money::fromMillimes(12345).applyPercentBp(1000).millimes(), 1235);
    // 7 % de 10,000 DT = 0,700 — exact
    QCOMPARE(Money::fromMillimes(10000).applyPercentBp(700).millimes(), 700);
    // 19 % de 1,000 DT = 0,190 — exact
    QCOMPARE(Money::fromMillimes(1000).applyPercentBp(1900).millimes(), 190);
    // Négatif : demi-éloigné de zéro
    QCOMPARE(Money::fromMillimes(-12345).applyPercentBp(1000).millimes(), -1235);
}

void TestMoney::displayFrench()
{
    const QString text =
        Money::fromMillimes(12500).toDisplayString(QLocale(QStringLiteral("fr_TN")));
    QVERIFY2(text.endsWith(QStringLiteral(" DT")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("12")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("500")), qPrintable(text));
}

void TestMoney::displayArabic()
{
    const QString text =
        Money::fromMillimes(12500).toDisplayString(QLocale(QStringLiteral("ar_TN")));
    QVERIFY2(text.contains(QStringLiteral("د.ت")), qPrintable(text));
}

void TestMoney::displayNegative()
{
    const QLocale locale(QStringLiteral("fr_TN"));
    const QString text = Money::fromMillimes(-5250).toDisplayString(locale);
    QVERIFY2(text.startsWith(locale.negativeSign()), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("250")), qPrintable(text));
}

QTEST_APPLESS_MAIN(TestMoney)
#include "tst_money.moc"
