#include "controllers/admin_controller.h"

#include "common/money.h"
#include "common/password_hasher.h"
#include "qrcodegen.hpp"

#include <QImage>
#include <QStandardPaths>

#include <QDate>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QLocale>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace nursera {
namespace {

QString roleCode(UserRole role)
{
    switch (role) {
    case UserRole::Manager: return QStringLiteral("manager");
    case UserRole::Seller: return QStringLiteral("seller");
    case UserRole::Worker: break;
    }
    return QStringLiteral("worker");
}

UserRole roleFromCode(const QString& code)
{
    if (code == QLatin1String("manager")) return UserRole::Manager;
    if (code == QLatin1String("seller")) return UserRole::Seller;
    return UserRole::Worker;
}

bool isValidPin(const QString& pin)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9]{4,6}$"));
    return pattern.match(pin).hasMatch();
}

QString usernameFrom(const QString& displayName)
{
    QString username = displayName.toLower().simplified();
    username.replace(QLatin1Char(' '), QLatin1Char('.'));
    return username;
}

const QStringList kCompanyKeys = {
    QStringLiteral("name"),    QStringLiteral("tagline"),
    QStringLiteral("address"), QStringLiteral("phone"),
    QStringLiteral("tax_id"),
};

QString kindCode(LocationKind kind)
{
    switch (kind) {
    case LocationKind::Field: return QStringLiteral("field");
    case LocationKind::SalesArea: return QStringLiteral("sales_area");
    case LocationKind::Warehouse: return QStringLiteral("warehouse");
    case LocationKind::Greenhouse: break;
    }
    return QStringLiteral("greenhouse");
}

LocationKind kindFromCode(const QString& code)
{
    if (code == QLatin1String("field")) return LocationKind::Field;
    if (code == QLatin1String("sales_area")) return LocationKind::SalesArea;
    if (code == QLatin1String("warehouse")) return LocationKind::Warehouse;
    return LocationKind::Greenhouse;
}

QString kindLabel(LocationKind kind)
{
    switch (kind) {
    case LocationKind::Field: return QStringLiteral("Parcelle");
    case LocationKind::SalesArea: return QStringLiteral("Zone de vente");
    case LocationKind::Warehouse: return QStringLiteral("Dépôt");
    case LocationKind::Greenhouse: break;
    }
    return QStringLiteral("Serre");
}

} // namespace

AdminController::AdminController(IUserRepository& users,
                                 ISettingsRepository& settings,
                                 BackupService& backups,
                                 QObject* parent)
    : QObject(parent)
    , m_userRepository(users)
    , m_settings(settings)
    , m_backupService(backups)
{
}

QString AdminController::backupDirUrl() const
{
    return QUrl::fromLocalFile(m_backupService.backupDir()).toString();
}

void AdminController::refresh()
{
    // Société (replis Pépinière Idéale, cohérents avec TicketGenerator)
    m_company = QVariantMap{
        {QStringLiteral("name"),
         m_settings.valueOr(QStringLiteral("company.name"),
                            QStringLiteral("Pépinière Idéale"))},
        {QStringLiteral("tagline"),
         m_settings.valueOr(QStringLiteral("company.tagline"),
                            QStringLiteral("Vente, Aménagement & Entretien"))},
        {QStringLiteral("address"),
         m_settings.valueOr(QStringLiteral("company.address"))},
        {QStringLiteral("phone"),
         m_settings.valueOr(QStringLiteral("company.phone"))},
        {QStringLiteral("tax_id"),
         m_settings.valueOr(QStringLiteral("company.tax_id"))},
    };

    // Timbre fiscal (F11-02) — défaut 1,000 DT
    m_stampDuty = m_settings.valueOr(QStringLiteral("finance.stamp_duty"),
                                     QStringLiteral("1,000"));

    // Tous les utilisateurs (actifs d'abord) — la désactivation est
    // réversible depuis ce même écran.
    m_users.clear();
    m_userCache.clear();
    if (const auto users = m_userRepository.allUsers()) {
        m_userCache = users.value();
        QList<User> sorted = m_userCache;
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const User& a, const User& b) {
                             return a.active > b.active;
                         });
        for (const User& user : sorted) {
            m_users.append(QVariantMap{
                {QStringLiteral("id"), user.id},
                {QStringLiteral("name"), user.displayName},
                {QStringLiteral("role"), roleCode(user.role)},
                {QStringLiteral("active"), user.active},
            });
        }
    }

    // Sauvegardes
    m_backups.clear();
    const QLocale locale;
    for (const BackupService::BackupInfo& backup : m_backupService.list()) {
        m_backups.append(QVariantMap{
            {QStringLiteral("fileName"), backup.fileName},
            {QStringLiteral("size"),
             locale.formattedDataSize(backup.sizeBytes)},
        });
    }

    // Référentiels (F11-06) — tous, actifs d'abord (désactivation réversible)
    m_categoryRows.clear();
    m_categoryCache.clear();
    if (m_categoryRepository) {
        if (const auto categories = m_categoryRepository->all(true)) {
            m_categoryCache = categories.value();
            QList<Category> sorted = m_categoryCache;
            std::stable_sort(sorted.begin(), sorted.end(),
                             [](const Category& a, const Category& b) {
                                 return a.active > b.active;
                             });
            for (const Category& category : sorted) {
                m_categoryRows.append(QVariantMap{
                    {QStringLiteral("id"), category.id},
                    {QStringLiteral("nameFr"), category.nameFr},
                    {QStringLiteral("nameAr"), category.nameAr},
                    {QStringLiteral("sortOrder"), category.sortOrder},
                    {QStringLiteral("active"), category.active},
                });
            }
        }
    }
    // Numérotations documentaires de l'année (F11-07) — informatif
    m_docNumbers.clear();
    {
        struct Doc { QString kind; QString label; QString prefix; int pad; };
        static const QList<Doc> kDocs = {
            {QStringLiteral("ticket"), tr("Ticket de caisse"), QStringLiteral("T"), 5},
            {QStringLiteral("invoice"), tr("Facture"), QStringLiteral("F"), 5},
            {QStringLiteral("credit_note"), tr("Avoir"), QStringLiteral("AV"), 3},
            {QStringLiteral("quote"), tr("Devis"), QStringLiteral("D"), 5},
            {QStringLiteral("po"), tr("Commande d'achat"), QStringLiteral("BC"), 3},
            {QStringLiteral("batch"), tr("Lot de production"), QStringLiteral("L"), 3},
        };
        const int year = QDate::currentDate().year();
        QHash<QString, int> next;
        if (const auto counters = m_settings.documentCounters(year))
            for (const DocCounter& counter : counters.value())
                next.insert(counter.kind, counter.nextNumber);
        for (const Doc& doc : kDocs) {
            m_docNumbers.append(QVariantMap{
                {QStringLiteral("label"), doc.label},
                {QStringLiteral("number"),
                 QStringLiteral("%1-%2-%3")
                     .arg(doc.prefix)
                     .arg(year)
                     .arg(next.value(doc.kind, 1), doc.pad, 10,
                          QLatin1Char('0'))},
            });
        }
    }

    m_locationRows.clear();
    m_locationCache.clear();
    if (m_locationRepository) {
        if (const auto locations = m_locationRepository->all(true)) {
            m_locationCache = locations.value();
            QList<Location> sorted = m_locationCache;
            std::stable_sort(sorted.begin(), sorted.end(),
                             [](const Location& a, const Location& b) {
                                 return a.active > b.active;
                             });
            for (const Location& location : sorted) {
                m_locationRows.append(QVariantMap{
                    {QStringLiteral("id"), location.id},
                    {QStringLiteral("nameFr"), location.nameFr},
                    {QStringLiteral("nameAr"), location.nameAr},
                    {QStringLiteral("kind"), kindCode(location.kind)},
                    {QStringLiteral("kindLabel"), kindLabel(location.kind)},
                    {QStringLiteral("active"), location.active},
                });
            }
        }
    }
    emit refreshed();
}

bool AdminController::saveCompany(const QVariantMap& data)
{
    for (const QString& key : kCompanyKeys) {
        if (const auto saved = m_settings.setValue(
                QStringLiteral("company.") + key,
                data.value(key).toString().trimmed());
            !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
    }
    refresh();
    emit companySaved();
    return true;
}

bool AdminController::saveStampDuty(const QString& amount)
{
    if (Money::parseMillimes(amount) < 0) {
        emit errorOccurred(tr("Montant invalide — format attendu : 1,000"));
        return false;
    }
    if (const auto saved =
            m_settings.setValue(QStringLiteral("finance.stamp_duty"), amount.trimmed());
        !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    refresh();
    emit companySaved();
    return true;
}

int AdminController::activeManagerCount() const
{
    int managers = 0;
    for (const User& user : m_userCache)
        if (user.role == UserRole::Manager && user.active)
            ++managers;
    return managers;
}

bool AdminController::createUser(const QVariantMap& data)
{
    User user;
    user.displayName = data.value(QStringLiteral("name")).toString().trimmed();
    if (user.displayName.isEmpty()) {
        emit errorOccurred(tr("Le nom est obligatoire."));
        return false;
    }
    const QString pin = data.value(QStringLiteral("pin")).toString();
    if (!isValidPin(pin)) {
        emit errorOccurred(tr("Le PIN doit contenir 4 à 6 chiffres."));
        return false;
    }
    if (pin != data.value(QStringLiteral("confirm")).toString()) {
        emit errorOccurred(tr("Les deux PIN ne correspondent pas."));
        return false;
    }
    user.username = usernameFrom(user.displayName);
    user.role = roleFromCode(data.value(QStringLiteral("role")).toString());

    const auto created = m_userRepository.insert(user, PasswordHasher::hash(pin));
    if (!created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit userSaved();
    return true;
}

bool AdminController::deactivateUser(int userId)
{
    for (const User& user : m_userCache) {
        if (user.id != userId)
            continue;
        // RG-01.a : il reste toujours au moins un Gérant actif
        if (user.role == UserRole::Manager && activeManagerCount() <= 1) {
            emit errorOccurred(
                tr("Impossible : il doit rester au moins un Gérant actif."));
            return false;
        }
        User updated = user;
        updated.active = false;
        if (const auto saved = m_userRepository.update(updated); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
        refresh();
        emit userSaved();
        return true;
    }
    emit errorOccurred(tr("Utilisateur introuvable."));
    return false;
}

bool AdminController::reactivateUser(int userId)
{
    for (const User& user : m_userCache) {
        if (user.id != userId)
            continue;
        User updated = user;
        updated.active = true;
        if (const auto saved = m_userRepository.update(updated); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
        refresh();
        emit userSaved();
        return true;
    }
    emit errorOccurred(tr("Utilisateur introuvable."));
    return false;
}

bool AdminController::resetPin(int userId, const QString& pin,
                               const QString& confirm)
{
    if (!isValidPin(pin)) {
        emit errorOccurred(tr("Le PIN doit contenir 4 à 6 chiffres."));
        return false;
    }
    if (pin != confirm) {
        emit errorOccurred(tr("Les deux PIN ne correspondent pas."));
        return false;
    }
    if (const auto saved =
            m_userRepository.setPin(userId, PasswordHasher::hash(pin));
        !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    emit userSaved();
    return true;
}

bool AdminController::syncEnabled() const
{
    return m_settings.valueOr(QStringLiteral("sync.master"))
        == QLatin1String("1");
}

QString AdminController::syncPort() const
{
    return m_settings.valueOr(QStringLiteral("sync.port"),
                              QStringLiteral("8477"));
}

QString AdminController::syncAddresses() const
{
    QStringList addresses;
    for (const QHostAddress& address : QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol
            && !address.isLoopback())
            addresses.append(address.toString());
    return addresses.join(QStringLiteral("  ·  "));
}

QString AdminController::syncQrUrl() const
{
    // Première IPv4 locale : la cible que le mobile doit joindre.
    QString host;
    for (const QHostAddress& address : QNetworkInterface::allAddresses())
        if (address.protocol() == QAbstractSocket::IPv4Protocol
            && !address.isLoopback()) {
            host = address.toString();
            break;
        }
    if (host.isEmpty())
        return {};

    using qrcodegen::QrCode;
    const QString payload =
        QStringLiteral("nursera://%1:%2").arg(host, syncPort());
    const QrCode qr = QrCode::encodeText(payload.toUtf8().constData(),
                                         QrCode::Ecc::MEDIUM);

    constexpr int kScale = 6;
    constexpr int kQuiet = 4; // marge silencieuse normative
    const int size = (qr.getSize() + kQuiet * 2) * kScale;
    QImage image(size, size, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < qr.getSize(); ++y)
        for (int x = 0; x < qr.getSize(); ++x)
            if (qr.getModule(x, y))
                for (int dy = 0; dy < kScale; ++dy)
                    for (int dx = 0; dx < kScale; ++dx)
                        image.setPixel((x + kQuiet) * kScale + dx,
                                       (y + kQuiet) * kScale + dy,
                                       0xFF000000);

    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/nursera-sync-qr.png");
    if (!image.save(path))
        return {};
    return QUrl::fromLocalFile(path).toString();
}

bool AdminController::saveSyncSettings(bool enabled, const QString& port)
{
    const int portNumber = port.toInt();
    if (portNumber <= 0 || portNumber > 65535) {
        emit errorOccurred(tr("Port invalide (1-65535)."));
        return false;
    }
    if (const auto saved = m_settings.setValue(
            QStringLiteral("sync.master"),
            enabled ? QStringLiteral("1") : QStringLiteral("0"));
        !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    if (const auto saved =
            m_settings.setValue(QStringLiteral("sync.port"), port);
        !saved) {
        emit errorOccurred(saved.error().message);
        return false;
    }
    refresh();
    return true;
}

bool AdminController::stageRestore(const QString& fileName)
{
    if (const auto staged = m_backupService.stageRestore(fileName); !staged) {
        emit errorOccurred(staged.error().message);
        return false;
    }
    refresh();
    return true;
}

void AdminController::cancelRestore()
{
    m_backupService.cancelPendingRestore();
    refresh();
}

int AdminController::activeSalesAreaCount() const
{
    int count = 0;
    for (const Location& location : m_locationCache)
        if (location.kind == LocationKind::SalesArea && location.active)
            ++count;
    return count;
}

bool AdminController::saveCategory(const QVariantMap& data)
{
    if (!m_categoryRepository)
        return false;
    Category category;
    category.id = data.value(QStringLiteral("id")).toInt();
    category.nameFr = data.value(QStringLiteral("nameFr")).toString().trimmed();
    category.nameAr = data.value(QStringLiteral("nameAr")).toString().trimmed();
    category.sortOrder = data.value(QStringLiteral("sortOrder")).toInt();
    if (category.nameFr.isEmpty()) {
        emit errorOccurred(tr("Le nom français est obligatoire."));
        return false;
    }
    if (category.id > 0) {
        // Conserver parent et statut existants — l'édition ne les touche pas.
        for (const Category& existing : m_categoryCache)
            if (existing.id == category.id) {
                category.parentId = existing.parentId;
                category.active = existing.active;
            }
        if (const auto saved = m_categoryRepository->update(category); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
    } else if (const auto created = m_categoryRepository->insert(category);
               !created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit referentialSaved();
    return true;
}

bool AdminController::setCategoryActive(int categoryId, bool active)
{
    if (!m_categoryRepository)
        return false;
    for (const Category& category : m_categoryCache) {
        if (category.id != categoryId)
            continue;
        Category updated = category;
        updated.active = active;
        if (const auto saved = m_categoryRepository->update(updated); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
        refresh();
        emit referentialSaved();
        return true;
    }
    emit errorOccurred(tr("Catégorie introuvable."));
    return false;
}

bool AdminController::categoryDeletable(int categoryId)
{
    if (!m_categoryRepository)
        return false;
    const auto referenced = m_categoryRepository->isReferenced(categoryId);
    return referenced.isOk() && !referenced.value();
}

bool AdminController::deleteCategory(int categoryId)
{
    if (!m_categoryRepository)
        return false;
    if (const auto removed = m_categoryRepository->remove(categoryId);
        !removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    refresh();
    emit referentialSaved();
    return true;
}

bool AdminController::saveLocation(const QVariantMap& data)
{
    if (!m_locationRepository)
        return false;
    Location location;
    location.id = data.value(QStringLiteral("id")).toInt();
    location.nameFr = data.value(QStringLiteral("nameFr")).toString().trimmed();
    location.nameAr = data.value(QStringLiteral("nameAr")).toString().trimmed();
    location.kind =
        kindFromCode(data.value(QStringLiteral("kind")).toString());
    if (location.nameFr.isEmpty()) {
        emit errorOccurred(tr("Le nom français est obligatoire."));
        return false;
    }
    if (location.id > 0) {
        for (const Location& existing : m_locationCache)
            if (existing.id == location.id) {
                location.active = existing.active;
                // La caisse exige une zone de vente active (RG-04.a).
                if (existing.kind == LocationKind::SalesArea
                    && location.kind != LocationKind::SalesArea
                    && existing.active && activeSalesAreaCount() <= 1) {
                    emit errorOccurred(
                        tr("Impossible : la caisse a besoin d'au moins une "
                           "zone de vente active."));
                    return false;
                }
            }
        if (const auto saved = m_locationRepository->update(location); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
    } else if (const auto created = m_locationRepository->insert(location);
               !created) {
        emit errorOccurred(created.error().message);
        return false;
    }
    refresh();
    emit referentialSaved();
    return true;
}

bool AdminController::setLocationActive(int locationId, bool active)
{
    if (!m_locationRepository)
        return false;
    for (const Location& location : m_locationCache) {
        if (location.id != locationId)
            continue;
        // La caisse exige une zone de vente active (RG-04.a).
        if (!active && location.kind == LocationKind::SalesArea
            && location.active && activeSalesAreaCount() <= 1) {
            emit errorOccurred(
                tr("Impossible : la caisse a besoin d'au moins une zone "
                   "de vente active."));
            return false;
        }
        Location updated = location;
        updated.active = active;
        if (const auto saved = m_locationRepository->update(updated); !saved) {
            emit errorOccurred(saved.error().message);
            return false;
        }
        refresh();
        emit referentialSaved();
        return true;
    }
    emit errorOccurred(tr("Emplacement introuvable."));
    return false;
}

bool AdminController::locationDeletable(int locationId)
{
    if (!m_locationRepository)
        return false;
    const auto referenced = m_locationRepository->isReferenced(locationId);
    return referenced.isOk() && !referenced.value();
}

bool AdminController::deleteLocation(int locationId)
{
    if (!m_locationRepository)
        return false;
    for (const Location& location : m_locationCache)
        if (location.id == locationId && location.active
            && location.kind == LocationKind::SalesArea
            && activeSalesAreaCount() <= 1) {
            emit errorOccurred(
                tr("Impossible : la caisse a besoin d'au moins une zone "
                   "de vente active."));
            return false;
        }
    if (const auto removed = m_locationRepository->remove(locationId);
        !removed) {
        emit errorOccurred(removed.error().message);
        return false;
    }
    refresh();
    emit referentialSaved();
    return true;
}

bool AdminController::backupNow()
{
    const auto backup = m_backupService.backupNow();
    if (!backup) {
        emit errorOccurred(backup.error().message);
        return false;
    }
    refresh();
    emit backupDone(QFileInfo(backup.value()).fileName());
    return true;
}

} // namespace nursera
