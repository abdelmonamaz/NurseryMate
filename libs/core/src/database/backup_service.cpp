#include "database/backup_service.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <utility>

namespace nursera {
namespace {

constexpr int kRetentionDays = 30;
const QString kPrefix = QStringLiteral("nursera-");

// Date encodée dans le nom du fichier : nursera-AAAAMMJJ-HHMMSS.db
QDate dateOfBackup(const QString& fileName)
{
    return QDate::fromString(fileName.mid(kPrefix.size(), 8),
                             QStringLiteral("yyyyMMdd"));
}

} // namespace

BackupService::BackupService(QString connectionName, QString backupDir)
    : m_connectionName(std::move(connectionName))
    , m_backupDir(std::move(backupDir))
{
}

Result<QString> BackupService::backupNow()
{
    if (!QDir().mkpath(m_backupDir))
        return Result<QString>::fail(QStringLiteral("backup.dir"), m_backupDir);

    const QString fileName = kPrefix
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QStringLiteral(".db");
    QString filePath = m_backupDir + QLatin1Char('/') + fileName;
    filePath.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Écrase une éventuelle sauvegarde de la même seconde (VACUUM INTO
    // refuse d'écrire sur un fichier existant)
    QFile::remove(filePath);

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("VACUUM INTO '%1'")
                        .arg(QString(filePath).replace(QLatin1Char('\''),
                                                       QStringLiteral("''")))))
        return Result<QString>::fail(QStringLiteral("backup.vacuum"),
                                     query.lastError().text());

    // Test d'intégrité après chaque sauvegarde (doc 02 §8)
    if (const auto ok = verifyIntegrity(filePath); !ok) {
        QFile::remove(filePath);
        return Result<QString>::fail(ok.error());
    }

    rotate();
    return Result<QString>::ok(filePath);
}

Result<void> BackupService::verifyIntegrity(const QString& filePath) const
{
    const QString checkConnection = QStringLiteral("backup_check");
    QString error;
    {
        QSqlDatabase db =
            QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), checkConnection);
        db.setDatabaseName(filePath);
        if (!db.open()) {
            error = db.lastError().text();
        } else {
            QSqlQuery query(db);
            if (!query.exec(QStringLiteral("PRAGMA integrity_check")) || !query.next()
                || query.value(0).toString() != QLatin1String("ok"))
                error = QStringLiteral("integrity_check a échoué");
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(checkConnection);

    if (!error.isEmpty())
        return Result<void>::fail(QStringLiteral("backup.integrity"), error);
    return Result<void>::ok();
}

void BackupService::rotate() const
{
    const QDate cutoff = QDate::currentDate().addDays(-kRetentionDays);
    const QDir dir(m_backupDir);
    const QStringList files =
        dir.entryList({kPrefix + QStringLiteral("*.db")}, QDir::Files);
    for (const QString& fileName : files) {
        const QDate date = dateOfBackup(fileName);
        if (date.isValid() && date < cutoff)
            QFile::remove(dir.filePath(fileName));
    }
}

Result<QString> BackupService::autoBackupIfDue(ISettingsRepository& settings)
{
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    if (settings.valueOr(QStringLiteral("backup.last_day")) == today)
        return Result<QString>::ok(QString()); // déjà faite aujourd'hui

    const auto backup = backupNow();
    if (!backup)
        return backup;
    settings.setValue(QStringLiteral("backup.last_day"), today);
    return backup;
}

QList<BackupService::BackupInfo> BackupService::list() const
{
    QList<BackupInfo> backups;
    const QDir dir(m_backupDir);
    const QFileInfoList files = dir.entryInfoList(
        {kPrefix + QStringLiteral("*.db")}, QDir::Files, QDir::Name | QDir::Reversed);
    for (const QFileInfo& info : files) {
        BackupInfo backup;
        backup.fileName = info.fileName();
        backup.filePath = info.absoluteFilePath();
        backup.sizeBytes = info.size();
        backups.append(backup);
    }
    return backups;
}

QString BackupService::pendingPath() const
{
    // À côté de la base : chemin de la base courante depuis la connexion.
    const QString databasePath =
        QSqlDatabase::database(m_connectionName).databaseName();
    return QFileInfo(databasePath).absolutePath()
        + QStringLiteral("/restore-pending.db");
}

Result<void> BackupService::stageRestore(const QString& fileName)
{
    const QString source = m_backupDir + QLatin1Char('/') + fileName;
    if (!QFile::exists(source))
        return Result<void>::fail(QStringLiteral("restore.notFound"),
                                  QStringLiteral("Sauvegarde introuvable."));
    if (const auto ok = verifyIntegrity(source); !ok)
        return Result<void>::fail(ok.error());

    const QString pending = pendingPath();
    QFile::remove(pending);
    if (!QFile::copy(source, pending))
        return Result<void>::fail(QStringLiteral("restore.copy"),
                                  QStringLiteral("Copie impossible vers %1.")
                                      .arg(pending));
    return Result<void>::ok();
}

bool BackupService::restorePending() const
{
    return QFile::exists(pendingPath());
}

void BackupService::cancelPendingRestore() const
{
    QFile::remove(pendingPath());
}

Result<bool> BackupService::applyPendingRestore(const QString& databasePath)
{
    const QString pending = QFileInfo(databasePath).absolutePath()
        + QStringLiteral("/restore-pending.db");
    if (!QFile::exists(pending))
        return Result<bool>::ok(false);

    // Filet de sécurité : la base remplacée est conservée en pre-restore-…
    if (QFile::exists(databasePath)) {
        const QString safety = QFileInfo(databasePath).absolutePath()
            + QStringLiteral("/pre-restore-")
            + QDateTime::currentDateTime().toString(
                  QStringLiteral("yyyyMMdd-HHmmss"))
            + QStringLiteral(".db");
        QFile::remove(safety);
        if (!QFile::copy(databasePath, safety))
            return Result<bool>::fail(
                QStringLiteral("restore.safety"),
                QStringLiteral("Copie de sécurité impossible — "
                               "restauration annulée."));
        QFile::remove(databasePath);
    }
    // Journaux SQLite d'une autre base : jamais réutilisés
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));

    if (!QFile::rename(pending, databasePath))
        return Result<bool>::fail(QStringLiteral("restore.apply"),
                                  QStringLiteral("Remplacement impossible."));
    return Result<bool>::ok(true);
}

} // namespace nursera
