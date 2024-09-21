#pragma once

#include <QString>

namespace nursera {

// Hachage des PIN/mots de passe (doc 02 §8) : PBKDF2-SHA256 via
// QPasswordDigestor, sel aléatoire par secret, comparaison à temps constant.
// Format stocké : "pbkdf2-sha256$<itérations>$<sel_b64>$<clé_b64>".
// (Argon2id via libsodium reste la cible V1.5 — le format préfixé permet
// la migration transparente des hachages.)
class PasswordHasher
{
public:
    static QString hash(const QString& secret);
    static bool verify(const QString& secret, const QString& stored);
};

} // namespace nursera
