#pragma once

#include "common/result.h"
#include "repositories/isettings_repository.h"

#include <QList>
#include <QString>

namespace nursera {

// Sauvegardes de la base (F11-04, doc 02 §8) :
//  - `VACUUM INTO` vers backups/nursera-AAAAMMJJ-HHMMSS.db
//  - contrôle d'intégrité du fichier produit (PRAGMA integrity_check)
//  - rotation : suppression des sauvegardes de plus de 30 jours
//  - automatique une fois par jour au démarrage (clé settings backup.last_day)
class BackupService
{
public:
    struct BackupInfo
    {
        QString fileName;
        QString filePath;
        qint64 sizeBytes = 0;
    };

    BackupService(QString connectionName, QString backupDir);

    // Sauvegarde immédiate ; retourne le chemin du fichier créé.
    Result<QString> backupNow();

    // Sauvegarde quotidienne : ne fait rien si déjà faite aujourd'hui.
    // Retourne le chemin, ou une chaîne vide si non due.
    Result<QString> autoBackupIfDue(ISettingsRepository& settings);

    // Sauvegardes présentes, plus récentes d'abord.
    QList<BackupInfo> list() const;

    QString backupDir() const { return m_backupDir; }

    // ── Restauration guidée (F11-04) ──────────────────────────
    // La base OUVERTE ne peut pas être remplacée à chaud : la sauvegarde
    // choisie (intégrité vérifiée) est copiée « en attente » à côté de la
    // base, puis appliquée par applyPendingRestore() AU PROCHAIN DÉMARRAGE.
    Result<void> stageRestore(const QString& fileName);
    bool restorePending() const;
    void cancelPendingRestore() const;

    // À appeler au démarrage AVANT d'ouvrir la base : si une restauration
    // est en attente, l'actuelle est d'abord copiée en pre-restore-….db
    // (rien n'est jamais perdu) puis remplacée. Retourne true si appliquée.
    static Result<bool> applyPendingRestore(const QString& databasePath);

private:
    Result<void> verifyIntegrity(const QString& filePath) const;
    void rotate() const;
    QString pendingPath() const;

    QString m_connectionName;
    QString m_backupDir;
};

} // namespace nursera
