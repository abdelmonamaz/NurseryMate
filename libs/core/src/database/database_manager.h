#pragma once

#include "common/result.h"

#include <QSqlDatabase>
#include <QString>

namespace nursera {

// Ouvre la base SQLite, applique les PRAGMA (WAL, foreign_keys) et les
// migrations embarquées :/migrations/NNN_*.sql — suivi par PRAGMA user_version.
// Doc 02 §3.2. L'accès depuis un thread d'E/S dédié (database_worker,
// pattern GestionScolaire) sera ajouté avec les premiers repositories.
class DatabaseManager
{
public:
    explicit DatabaseManager(QString databasePath,
                             QString connectionName = QStringLiteral("nursera_main"));
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    Result<void> open();
    QSqlDatabase database() const;
    QString connectionName() const { return m_connectionName; }
    int schemaVersion() const;

private:
    Result<void> applyPragmas();
    Result<void> applyMigrations();

    QString m_databasePath;
    QString m_connectionName;
};

} // namespace nursera
