#include "controllers/admin_controller.h"
#include "controllers/auth_controller.h"
#include "controllers/batch_controller.h"
#include "controllers/catalog_controller.h"
#include "controllers/customer_controller.h"
#include "controllers/dashboard_controller.h"
#include "controllers/inventory_controller.h"
#include "controllers/invoice_controller.h"
#include "controllers/quote_controller.h"
#include "controllers/reminder_controller.h"
#include "controllers/report_controller.h"
#include "controllers/sale_controller.h"
#include "controllers/stock_controller.h"
#include "controllers/supplier_controller.h"
#include "database/backup_service.h"
#include "database/database_manager.h"
#include "repositories/sqlite/sqlite_batch_repository.h"
#include "repositories/sqlite/sqlite_category_repository.h"
#include "repositories/sqlite/sqlite_customer_repository.h"
#include "repositories/sqlite/sqlite_dashboard_repository.h"
#include "repositories/sqlite/sqlite_inventory_repository.h"
#include "repositories/sqlite/sqlite_invoice_repository.h"
#include "repositories/sqlite/sqlite_location_repository.h"
#include "repositories/sqlite/sqlite_product_repository.h"
#include "repositories/sqlite/sqlite_quote_repository.h"
#include "repositories/sqlite/sqlite_reminder_repository.h"
#include "repositories/sqlite/sqlite_report_repository.h"
#include "repositories/sqlite/sqlite_sale_repository.h"
#include "repositories/sqlite/sqlite_settings_repository.h"
#include "repositories/sqlite/sqlite_stock_repository.h"
#include "repositories/sqlite/sqlite_supplier_repository.h"
#include "repositories/sqlite/sqlite_user_repository.h"

#ifdef NURSERA_HAS_SYNC
#include "sync_server.h"
#include <memory>
#endif

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Pepiniere Ideale"));
    QGuiApplication::setApplicationName(QStringLiteral("Nursera"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION_STR));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Medium.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Bold.ttf"));
    QGuiApplication::setFont(QFont(QStringLiteral("Inter")));

    // Données dans %APPDATA%/Pepiniere Ideale/Nursera (doc 02 §9)
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);

    // Restauration guidée (F11-04) : appliquée avant l'ouverture de la
    // base — l'ancienne est conservée en pre-restore-….db.
    if (const auto restored = nursera::BackupService::applyPendingRestore(
            dataDir + QStringLiteral("/nursera.db"));
        !restored)
        qWarning("Restauration non appliquée : %s",
                 qPrintable(restored.error().message));

    nursera::DatabaseManager db(dataDir + QStringLiteral("/nursera.db"));
    if (auto opened = db.open(); !opened) {
        qCritical("Base de données inaccessible [%s] %s",
                  qPrintable(opened.error().code),
                  qPrintable(opened.error().message));
        return 1;
    }

    // Composition manuelle (doc 02 §2.2) : repositories -> controllers.
    nursera::SqliteProductRepository productRepository(db.connectionName());
    nursera::SqliteCategoryRepository categoryRepository(db.connectionName());
    nursera::SqliteLocationRepository locationRepository(db.connectionName());
    nursera::SqliteStockRepository stockRepository(db.connectionName());

    nursera::CatalogController catalogController(productRepository,
                                                 categoryRepository);
    catalogController.setPhotoDir(dataDir + QStringLiteral("/photos"));
    catalogController.setLabelDir(dataDir + QStringLiteral("/labels"));
    nursera::StockController stockController(stockRepository,
                                             locationRepository);
    nursera::SqliteInventoryRepository inventoryRepository(db.connectionName(),
                                                           stockRepository);
    nursera::InventoryController inventoryController(inventoryRepository,
                                                     locationRepository);
    nursera::SqliteUserRepository userRepository(db.connectionName());
    nursera::AuthController authController(userRepository);
    nursera::SqliteSaleRepository saleRepository(db.connectionName(),
                                                 stockRepository);
    nursera::SqliteSettingsRepository settingsRepository(db.connectionName());
    nursera::SaleController saleController(saleRepository, stockRepository,
                                           locationRepository);
    // Tickets PDF dans %APPDATA%/…/tickets (F04-06), avoirs dans …/avoirs
    saleController.configureTickets(&settingsRepository,
                                    dataDir + QStringLiteral("/tickets"),
                                    dataDir + QStringLiteral("/avoirs"));

    nursera::SqliteDashboardRepository dashboardRepository(db.connectionName());
    nursera::DashboardController dashboardController(dashboardRepository);
    nursera::SqliteCustomerRepository customerRepository(db.connectionName());
    nursera::CustomerController customerController(customerRepository);
    saleController.configureCustomers(&customerRepository);
    nursera::SqliteSupplierRepository supplierRepository(db.connectionName(),
                                                         stockRepository);
    nursera::SupplierController supplierController(supplierRepository);

    // Sauvegarde automatique quotidienne (F11-04) — non bloquante
    nursera::BackupService backupService(db.connectionName(),
                                         dataDir + QStringLiteral("/backups"));
    if (const auto backup = backupService.autoBackupIfDue(settingsRepository);
        !backup)
        qWarning("Sauvegarde automatique échouée : %s",
                 qPrintable(backup.error().message));

    nursera::AdminController adminController(userRepository, settingsRepository,
                                             backupService);
    // Référentiels catégories/emplacements dans Réglages (F11-06)
    adminController.configureReferentials(&categoryRepository,
                                          &locationRepository);
    nursera::SqliteInvoiceRepository invoiceRepository(db.connectionName());
    nursera::InvoiceController invoiceController(
        invoiceRepository, settingsRepository,
        dataDir + QStringLiteral("/invoices"));
    nursera::SqliteBatchRepository batchRepository(db.connectionName(),
                                                   stockRepository);
    nursera::BatchController batchController(batchRepository, productRepository,
                                             locationRepository);
    nursera::SqliteQuoteRepository quoteRepository(db.connectionName());
    nursera::QuoteController quoteController(
        quoteRepository, stockRepository, settingsRepository,
        dataDir + QStringLiteral("/quotes"));
    // Devis accepté -> chargé dans le panier de caisse (F05)
    saleController.configureQuotes(&quoteRepository);
    nursera::SqliteReportRepository reportRepository(db.connectionName());
    nursera::ReportController reportController(
        reportRepository, settingsRepository,
        dataDir + QStringLiteral("/exports"));

    // Rappels d'entretien (F08-04)
    nursera::SqliteReminderRepository reminderRepository(db.connectionName());
    nursera::ReminderController reminderController(reminderRepository);

    // Attribution des saisies à l'utilisateur connecté (F01-04)
    const auto currentUserId =
        [&authController] { return authController.currentUserId(); };
    stockController.setUserIdProvider(currentUserId);
    reminderController.setUserIdProvider(currentUserId);
    saleController.setUserIdProvider(currentUserId);
    customerController.setUserIdProvider(currentUserId);
    supplierController.setUserIdProvider(currentUserId);
    invoiceController.setUserIdProvider(currentUserId);
    batchController.setUserIdProvider(currentUserId);
    quoteController.setUserIdProvider(currentUserId);

    // Connexion auto de développement (NURSERA_DEV_USER + NURSERA_DEV_PIN) —
    // passe par le vrai chemin de vérification du PIN.
    if (const QString devUser = qEnvironmentVariable("NURSERA_DEV_USER");
        !devUser.isEmpty()) {
        if (const auto users = userRepository.activeUsers()) {
            for (const nursera::User& user : users.value()) {
                if (user.username == devUser) {
                    authController.login(user.id,
                                         qEnvironmentVariable("NURSERA_DEV_PIN"));
                    break;
                }
            }
        }
    }

    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Auth", &authController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Sales", &saleController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Dashboard",
                                 &dashboardController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Customers",
                                 &customerController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Suppliers",
                                 &supplierController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Admin",
                                 &adminController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Invoices",
                                 &invoiceController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Production",
                                 &batchController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Quotes",
                                 &quoteController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Reports",
                                 &reportController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Reminders",
                                 &reminderController);

    // Support/dev : génère la facture d'une vente et l'ouvre
    // (NURSERA_GEN_INVOICE=<saleId>).
    if (const int devSaleId = qEnvironmentVariableIntValue("NURSERA_GEN_INVOICE");
        devSaleId > 0) {
        const QString url = invoiceController.invoiceForSale(devSaleId);
        qInfo("Facture générée : %s", qPrintable(url));
    }
    // Support/dev : planche d'étiquettes d'un produit (NURSERA_GEN_LABELS=<id>).
    if (const int devProductId = qEnvironmentVariableIntValue("NURSERA_GEN_LABELS");
        devProductId > 0) {
        const QString url = catalogController.printLabels(devProductId);
        qInfo("Étiquettes générées : %s", qPrintable(url));
    }
    // Support/dev : rapport PDF (NURSERA_GEN_REPORT=sales_category|sales_payment
    // |stock|losses).
    if (const QString devReport = qEnvironmentVariable("NURSERA_GEN_REPORT");
        !devReport.isEmpty()) {
        const QString url = reportController.exportPdf(devReport);
        qInfo("Rapport PDF généré : %s", qPrintable(url));
    }
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Catalog",
                                 &catalogController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Stock",
                                 &stockController);
    qmlRegisterSingletonInstance("Nursera.App", 1, 0, "Inventory",
                                 &inventoryController);

#ifdef NURSERA_HAS_SYNC
    // Serveur LAN de sync mobile (M09) — poste principal uniquement
    std::unique_ptr<nursera::SyncServer> syncServer;
    if (settingsRepository.valueOr(QStringLiteral("sync.master"))
        == QLatin1String("1")) {
        syncServer = std::make_unique<nursera::SyncServer>(
            db.connectionName(), userRepository, stockRepository);
        const quint16 port = settingsRepository
                                 .valueOr(QStringLiteral("sync.port"),
                                          QStringLiteral("8477"))
                                 .toUShort();
        if (const auto started = syncServer->start(port); !started)
            qWarning("Serveur de sync non démarré : %s",
                     qPrintable(started.error().message));
        else
            qInfo("Serveur de sync LAN actif sur le port %u",
                  unsigned(started.value()));
    }
#endif

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Nursera.Desktop", "Main");

    return app.exec();
}
