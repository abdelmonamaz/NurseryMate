#include "database/database_manager.h"

#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>
#include <utility>

namespace nursera {
namespace {

bool isCommentOnly(const QString& fragment)
{
    const QStringList lines = fragment.split(QLatin1Char('\n'));
    return std::all_of(lines.cbegin(), lines.cend(), [](const QString& line) {
        const QString trimmed = line.trimmed();
        return trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("--"));
    });
}

} // namespace

DatabaseManager::DatabaseManager(QString databasePath, QString connectionName)
    : m_databasePath(std::move(databasePath))
    , m_connectionName(std::move(connectionName))
{
}

DatabaseManager::~DatabaseManager()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName, /*open=*/false).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QSqlDatabase DatabaseManager::database() const
{
    return QSqlDatabase::database(m_connectionName);
}

Result<void> DatabaseManager::open()
{
    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
        ? QSqlDatabase::database(m_connectionName, /*open=*/false)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open())
        return Result<void>::fail(QStringLiteral("db.open"), db.lastError().text());

    if (auto pragmas = applyPragmas(); !pragmas)
        return pragmas;
    return applyMigrations();
}

Result<void> DatabaseManager::applyPragmas()
{
    QSqlQuery query(database());
    const QStringList pragmas = {
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA foreign_keys=ON"),
        QStringLiteral("PRAGMA busy_timeout=5000"),
    };
    for (const QString& pragma : pragmas) {
        if (!query.exec(pragma))
            return Result<void>::fail(QStringLiteral("db.pragma"),
                                      query.lastError().text());
    }
    return Result<void>::ok();
}

int DatabaseManager::schemaVersion() const
{
    QSqlQuery query(database());
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        return query.value(0).toInt();
    return -1;
}

Result<void> DatabaseManager::applyMigrations()
{
    const QDir migrationsDir(QStringLiteral(":/migrations"));
    const QStringList files = migrationsDir.entryList(QDir::Files, QDir::Name);

    QSqlDatabase db = database();
    for (const QString& fileName : files) {
        bool numbered = false;
        const int version = fileName.left(3).toInt(&numbered);
        if (!numbered || version <= schemaVersion())
            continue;

        QFile file(migrationsDir.filePath(fileName));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return Result<void>::fail(QStringLiteral("db.migration.read"), fileName);
        const QString sql = QString::fromUtf8(file.readAll());

        if (!db.transaction())
            return Result<void>::fail(QStringLiteral("db.migration.tx"),
                                      db.lastError().text());

        QSqlQuery query(db);
        const QStringList statements = sql.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString& statement : statements) {
            const QString trimmed = statement.trimmed();
            if (trimmed.isEmpty() || isCommentOnly(trimmed))
                continue;
            if (!query.exec(trimmed)) {
                db.rollback();
                return Result<void>::fail(
                    QStringLiteral("db.migration.exec"),
                    QStringLiteral("%1 : %2").arg(fileName, query.lastError().text()));
            }
        }
        query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version));
        if (!db.commit()) {
            db.rollback();
            return Result<void>::fail(QStringLiteral("db.migration.commit"),
                                      db.lastError().text());
        }
    }
    return Result<void>::ok();
}

} // namespace nursera
