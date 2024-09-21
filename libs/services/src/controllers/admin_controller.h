#pragma once

#include "database/backup_service.h"
#include "repositories/icategory_repository.h"
#include "repositories/ilocation_repository.h"
#include "repositories/isettings_repository.h"
#include "repositories/iuser_repository.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace nursera {

// Controller de l'écran Réglages (M11) : société, utilisateurs, sauvegardes.
class AdminController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap company READ company NOTIFY refreshed)
    Q_PROPERTY(QString stampDuty READ stampDuty NOTIFY refreshed)
    Q_PROPERTY(QVariantList users READ users NOTIFY refreshed)
    Q_PROPERTY(QVariantList backups READ backups NOTIFY refreshed)
    Q_PROPERTY(QString backupDirUrl READ backupDirUrl CONSTANT)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY refreshed)
    Q_PROPERTY(QVariantList locations READ locations NOTIFY refreshed)
    Q_PROPERTY(QVariantList docNumbers READ docNumbers NOTIFY refreshed)
    Q_PROPERTY(bool restorePending READ restorePending NOTIFY refreshed)
    Q_PROPERTY(bool syncEnabled READ syncEnabled NOTIFY refreshed)
    Q_PROPERTY(QString syncPort READ syncPort NOTIFY refreshed)
    Q_PROPERTY(QString syncAddresses READ syncAddresses CONSTANT)

public:
    AdminController(IUserRepository& users,
                    ISettingsRepository& settings,
                    BackupService& backups,
                    QObject* parent = nullptr);

    // Référentiels (F11-06) : catégories et emplacements gérés depuis
    // l'écran Réglages.
    void configureReferentials(ICategoryRepository* categories,
                               ILocationRepository* locations)
    {
        m_categoryRepository = categories;
        m_locationRepository = locations;
    }

    QVariantMap company() const { return m_company; }
    QString stampDuty() const { return m_stampDuty; }
    QVariantList users() const { return m_users; }
    QVariantList backups() const { return m_backups; }
    QString backupDirUrl() const;
    QVariantList categories() const { return m_categoryRows; }
    QVariantList locations() const { return m_locationRows; }
    QVariantList docNumbers() const { return m_docNumbers; }

    Q_INVOKABLE void refresh();

    // Paramètres société (F11-01) — alimentent tickets et documents.
    Q_INVOKABLE bool saveCompany(const QVariantMap& data);

    // Paramètres financiers : timbre fiscal (F11-02).
    Q_INVOKABLE bool saveStampDuty(const QString& amount);

    // Utilisateurs (F01-02, RG-01.a/b)
    Q_INVOKABLE bool createUser(const QVariantMap& data);
    Q_INVOKABLE bool deactivateUser(int userId);
    // Réactivation — la désactivation est toujours réversible.
    Q_INVOKABLE bool reactivateUser(int userId);
    Q_INVOKABLE bool resetPin(int userId, const QString& pin,
                              const QString& confirm);

    // Sauvegardes (F11-04)
    Q_INVOKABLE bool backupNow();

    // Restauration guidée : mise en attente (intégrité vérifiée), appliquée
    // au prochain démarrage — annulable tant que l'app n'a pas redémarré.
    bool restorePending() const { return m_backupService.restorePending(); }
    Q_INVOKABLE bool stageRestore(const QString& fileName);
    Q_INVOKABLE void cancelRestore();

    // Poste principal : serveur LAN de sync mobile (M09) — appliqué au
    // prochain démarrage de l'application.
    bool syncEnabled() const;
    QString syncPort() const;
    QString syncAddresses() const; // IP locales à saisir sur le mobile
    Q_INVOKABLE bool saveSyncSettings(bool enabled, const QString& port);
    // QR d'appairage (F09-09) : nursera://IP:PORT rendu en PNG temporaire
    // — le mobile le scannera ; en attendant il documente la connexion.
    Q_INVOKABLE QString syncQrUrl() const;

    // Référentiels (F11-06) — norme : suppression seulement si jamais
    // référencé, sinon désactivation (réversible).
    Q_INVOKABLE bool saveCategory(const QVariantMap& data); // id 0 = création
    Q_INVOKABLE bool setCategoryActive(int categoryId, bool active);
    Q_INVOKABLE bool categoryDeletable(int categoryId);
    Q_INVOKABLE bool deleteCategory(int categoryId);
    Q_INVOKABLE bool saveLocation(const QVariantMap& data); // id 0 = création
    Q_INVOKABLE bool setLocationActive(int locationId, bool active);
    Q_INVOKABLE bool locationDeletable(int locationId);
    Q_INVOKABLE bool deleteLocation(int locationId);

signals:
    void refreshed();
    void companySaved();
    void userSaved();
    void backupDone(const QString& fileName);
    void referentialSaved();
    void errorOccurred(const QString& message);

private:
    int activeManagerCount() const;
    int activeSalesAreaCount() const;

    IUserRepository& m_userRepository;
    ISettingsRepository& m_settings;
    BackupService& m_backupService;
    ICategoryRepository* m_categoryRepository = nullptr;
    ILocationRepository* m_locationRepository = nullptr;
    QVariantMap m_company;
    QString m_stampDuty;
    QVariantList m_users;
    QVariantList m_backups;
    QVariantList m_categoryRows;
    QVariantList m_locationRows;
    QVariantList m_docNumbers;
    QList<User> m_userCache;
    QList<Category> m_categoryCache;
    QList<Location> m_locationCache;
};

} // namespace nursera
