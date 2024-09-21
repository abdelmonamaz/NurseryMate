#include "common/amount_to_words.h"

#include <QStringList>

#include <array>

namespace nursera {
namespace {

const std::array<QString, 17> kSmall = {
    QStringLiteral("zéro"),   QStringLiteral("un"),      QStringLiteral("deux"),
    QStringLiteral("trois"),  QStringLiteral("quatre"),  QStringLiteral("cinq"),
    QStringLiteral("six"),    QStringLiteral("sept"),    QStringLiteral("huit"),
    QStringLiteral("neuf"),   QStringLiteral("dix"),     QStringLiteral("onze"),
    QStringLiteral("douze"),  QStringLiteral("treize"),  QStringLiteral("quatorze"),
    QStringLiteral("quinze"), QStringLiteral("seize"),
};

// 0..99 ; `plural` : "quatre-vingts" (true) vs "quatre-vingt" (false, avant
// un autre nombre ou "mille").
QString below100(int n, bool plural)
{
    if (n < 17)
        return kSmall[n];
    if (n < 20)
        return QStringLiteral("dix-") + kSmall[n - 10];

    if (n < 70) {
        static const std::array<QString, 7> tens = {
            {{}, {}, QStringLiteral("vingt"), QStringLiteral("trente"),
             QStringLiteral("quarante"), QStringLiteral("cinquante"),
             QStringLiteral("soixante")}};
        const int t = n / 10;
        const int u = n % 10;
        if (u == 0)
            return tens[t];
        if (u == 1)
            return tens[t] + QStringLiteral(" et un");
        return tens[t] + QLatin1Char('-') + kSmall[u];
    }

    if (n < 80) {
        if (n == 71)
            return QStringLiteral("soixante et onze");
        return QStringLiteral("soixante-") + below100(n - 60, false);
    }

    // 80..99
    const int u = n - 80;
    if (u == 0)
        return plural ? QStringLiteral("quatre-vingts")
                      : QStringLiteral("quatre-vingt");
    return QStringLiteral("quatre-vingt-") + below100(u, false);
}

// 0..999
QString below1000(int n, bool plural)
{
    if (n < 100)
        return below100(n, plural);

    const int h = n / 100;
    const int rem = n % 100;
    QString hundred;
    if (rem == 0)
        hundred = (h == 1) ? QStringLiteral("cent")
                           : below100(h, false) + QStringLiteral(" cent")
                                 + (plural ? QStringLiteral("s") : QString());
    else
        hundred = (h == 1) ? QStringLiteral("cent")
                           : below100(h, false) + QStringLiteral(" cent");

    if (rem == 0)
        return hundred;
    return hundred + QLatin1Char(' ') + below100(rem, plural);
}

} // namespace

QString AmountToWords::frenchNumber(qint64 n)
{
    if (n == 0)
        return kSmall[0];
    if (n < 0)
        return QStringLiteral("moins ") + frenchNumber(-n);

    const qint64 milliards = n / 1000000000;
    n %= 1000000000;
    const qint64 millions = n / 1000000;
    n %= 1000000;
    const qint64 milliers = n / 1000;
    const int units = static_cast<int>(n % 1000);

    QStringList parts;
    if (milliards > 0)
        parts << below1000(static_cast<int>(milliards), true)
                     + QStringLiteral(" milliard")
                     + (milliards > 1 ? QStringLiteral("s") : QString());
    if (millions > 0)
        parts << below1000(static_cast<int>(millions), true)
                     + QStringLiteral(" million")
                     + (millions > 1 ? QStringLiteral("s") : QString());
    if (milliers > 0)
        parts << (milliers == 1
                      ? QStringLiteral("mille")
                      : below1000(static_cast<int>(milliers), false)
                            + QStringLiteral(" mille"));
    if (units > 0)
        parts << below1000(units, true);

    return parts.join(QLatin1Char(' '));
}

QString AmountToWords::frenchAmount(Money amount)
{
    const bool negative = amount.isNegative();
    const qint64 millimes = qAbs(amount.millimes());
    const qint64 dinars = millimes / 1000;
    const qint64 mils = millimes % 1000;

    QStringList parts;
    if (dinars > 0)
        parts << frenchNumber(dinars)
                     + (dinars == 1 ? QStringLiteral(" dinar")
                                    : QStringLiteral(" dinars"));
    if (mils > 0)
        parts << frenchNumber(mils)
                     + (mils == 1 ? QStringLiteral(" millime")
                                  : QStringLiteral(" millimes"));

    QString result = parts.isEmpty() ? QStringLiteral("zéro dinar")
                                      : parts.join(QStringLiteral(" et "));
    if (negative)
        result.prepend(QStringLiteral("moins "));
    result[0] = result[0].toUpper();
    return result;
}

} // namespace nursera
