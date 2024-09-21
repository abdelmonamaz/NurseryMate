#include "common/money.h"

#include <cstdlib>

namespace nursera {

Money Money::applyPercentBp(int basisPoints) const
{
    // Demi-supérieur (demi-éloigné de zéro pour les montants négatifs).
    const qint64 numerator = m_millimes * basisPoints;
    const qint64 rounded = numerator >= 0 ? (numerator + 5000) / 10000
                                          : -((-numerator + 5000) / 10000);
    return Money(rounded);
}

Money Money::vatFromTtc(int ratePercent) const
{
    if (ratePercent <= 0)
        return Money(0);
    // HT = TTC * 100 / (100 + taux), arrondi demi-supérieur
    const qint64 divisor = 100 + ratePercent;
    const qint64 ht = (m_millimes * 100 + divisor / 2) / divisor;
    return Money(m_millimes - ht);
}

qint64 Money::parseMillimes(QString text)
{
    text = text.trimmed();
    text.remove(QLatin1Char(' '));
    text.replace(QLatin1Char(','), QLatin1Char('.'));
    if (text.isEmpty() || text.startsWith(QLatin1Char('-')))
        return -1;

    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() > 2)
        return -1;

    qint64 dinars = 0;
    if (!parts.at(0).isEmpty()) {
        bool ok = false;
        dinars = parts.at(0).toLongLong(&ok);
        if (!ok)
            return -1;
    }

    qint64 millimes = 0;
    if (parts.size() == 2) {
        QString fraction = parts.at(1).left(3);
        if (fraction.isEmpty())
            return -1;
        while (fraction.size() < 3)
            fraction += QLatin1Char('0');
        bool ok = false;
        millimes = fraction.toLongLong(&ok);
        if (!ok)
            return -1;
    }
    return dinars * 1000 + millimes;
}

QString Money::toDisplayString(const QLocale& locale) const
{
    const qint64 absMillimes = std::llabs(m_millimes);
    QString text = locale.toString(static_cast<qlonglong>(absMillimes / 1000));
    text += locale.decimalPoint();
    text += QStringLiteral("%1").arg(absMillimes % 1000, 3, 10, QLatin1Char('0'));
    if (m_millimes < 0)
        text.prepend(locale.negativeSign());

    const bool arabic = locale.language() == QLocale::Arabic;
    return arabic ? text + QStringLiteral(" د.ت")
                  : text + QStringLiteral(" DT");
}

} // namespace nursera
