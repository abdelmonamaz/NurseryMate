#pragma once

#include <QLocale>
#include <QString>

#include <compare>

namespace nursera {

// Montant en TND stocké en millimes (entier 64 bits).
// Interdiction du double pour l'argent (RT-01, doc 02 §2.3).
class Money
{
public:
    constexpr Money() = default;

    static constexpr Money fromMillimes(qint64 millimes) { return Money(millimes); }

    // dinars >= 0 attendu avec millimes dans [0, 999] ; pour un montant
    // négatif, utiliser fromMillimes ou l'opérateur unaire.
    static constexpr Money fromTnd(qint64 dinars, int millimes = 0)
    {
        return Money(dinars * 1000 + (dinars < 0 ? -millimes : millimes));
    }

    constexpr qint64 millimes() const { return m_millimes; }
    constexpr bool isNegative() const { return m_millimes < 0; }

    constexpr Money operator+(Money other) const { return Money(m_millimes + other.m_millimes); }
    constexpr Money operator-(Money other) const { return Money(m_millimes - other.m_millimes); }
    constexpr Money operator-() const { return Money(-m_millimes); }
    constexpr Money operator*(qint64 quantity) const { return Money(m_millimes * quantity); }
    constexpr bool operator==(const Money&) const = default;
    constexpr auto operator<=>(const Money&) const = default;

    // Applique un pourcentage exprimé en points de base (100 bp = 1 %).
    // Arrondi demi-supérieur au millime (RT-01).
    Money applyPercentBp(int basisPoints) const;

    // Part de TVA contenue dans un montant TTC au taux donné (%).
    // Ex. 11,900 TTC à 19 % -> 1,900 de TVA.
    Money vatFromTtc(int ratePercent) const;

    // "12,500 DT" (fr) / "12,500 د.ت" (ar) — RT-01.
    QString toDisplayString(const QLocale& locale) const;

    // Parse "12,500" / "12.500" / "12" en millimes, sans double.
    // Retourne -1 si invalide (négatif, format inconnu).
    static qint64 parseMillimes(QString text);

private:
    explicit constexpr Money(qint64 millimes) : m_millimes(millimes) {}

    qint64 m_millimes = 0;
};

} // namespace nursera
