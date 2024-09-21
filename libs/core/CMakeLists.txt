# nursera_core : common (Result, Money), database (manager + migrations)
# Équivalent de GS_Core dans GestionScolaire — grandira avec models/ et repositories/.

qt_add_library(nursera_core STATIC
    src/common/result.h
    src/common/money.h
    src/common/money.cpp
    src/common/amount_to_words.h
    src/common/amount_to_words.cpp
    src/common/password_hasher.h
    src/common/password_hasher.cpp

    src/models/batch.h
    src/models/category.h
    src/models/customer.h
    src/models/dashboard.h
    src/models/inventory.h
    src/models/invoice.h
    src/models/location.h
    src/models/product.h
    src/models/quote.h
    src/models/report.h
    src/models/sale.h
    src/models/stock.h
    src/models/supplier.h
    src/models/user.h

    src/repositories/ibatch_repository.h
    src/repositories/icategory_repository.h
    src/repositories/icustomer_repository.h
    src/repositories/idashboard_repository.h
    src/repositories/iinventory_repository.h
    src/repositories/iinvoice_repository.h
    src/repositories/ilocation_repository.h
    src/repositories/iproduct_repository.h
    src/repositories/iquote_repository.h
    src/repositories/ireport_repository.h
    src/repositories/isale_repository.h
    src/repositories/isettings_repository.h
    src/repositories/istock_repository.h
    src/repositories/isupplier_repository.h
    src/repositories/iuser_repository.h
    src/repositories/sqlite/sqlite_batch_repository.h
    src/repositories/sqlite/sqlite_batch_repository.cpp
    src/repositories/sqlite/sqlite_category_repository.h
    src/repositories/sqlite/sqlite_category_repository.cpp
    src/repositories/sqlite/sqlite_customer_repository.h
    src/repositories/sqlite/sqlite_customer_repository.cpp
    src/repositories/sqlite/sqlite_dashboard_repository.h
    src/repositories/sqlite/sqlite_dashboard_repository.cpp
    src/repositories/sqlite/sqlite_inventory_repository.h
    src/repositories/sqlite/sqlite_inventory_repository.cpp
    src/repositories/sqlite/sqlite_invoice_repository.h
    src/repositories/sqlite/sqlite_invoice_repository.cpp
    src/repositories/sqlite/sqlite_location_repository.h
    src/repositories/sqlite/sqlite_location_repository.cpp
    src/repositories/sqlite/sqlite_product_repository.h
    src/repositories/sqlite/sqlite_product_repository.cpp
    src/repositories/sqlite/sqlite_quote_repository.h
    src/repositories/sqlite/sqlite_quote_repository.cpp
    src/repositories/sqlite/sqlite_report_repository.h
    src/repositories/sqlite/sqlite_report_repository.cpp
    src/repositories/ireminder_repository.h
    src/repositories/sqlite/sqlite_reminder_repository.h
    src/repositories/sqlite/sqlite_reminder_repository.cpp
    src/repositories/sqlite/sqlite_sale_repository.h
    src/repositories/sqlite/sqlite_sale_repository.cpp
    src/repositories/sqlite/sqlite_settings_repository.h
    src/repositories/sqlite/sqlite_settings_repository.cpp
    src/repositories/sqlite/sqlite_stock_repository.h
    src/repositories/sqlite/sqlite_stock_repository.cpp
    src/repositories/sqlite/sqlite_supplier_repository.h
    src/repositories/sqlite/sqlite_supplier_repository.cpp
    src/repositories/sqlite/sqlite_user_repository.h
    src/repositories/sqlite/sqlite_user_repository.cpp

    src/database/database_manager.h
    src/database/database_manager.cpp
    src/database/backup_service.h
    src/database/backup_service.cpp
)

target_include_directories(nursera_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Network : QPasswordDigestor (PBKDF2, password_hasher.cpp)
target_link_libraries(nursera_core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)

# Migrations SQL embarquées, appliquées au démarrage (doc 02 §3.2)
qt_add_resources(nursera_core "migrations"
    PREFIX "/migrations"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/migrations"
    FILES
        migrations/001_initial.sql
        migrations/002_seed_categories.sql
        migrations/003_seed_locations.sql
        migrations/004_inventories.sql
        migrations/005_sales.sql
        migrations/006_customers.sql
        migrations/007_suppliers.sql
        migrations/008_invoices.sql
        migrations/009_seed_jardinerie.sql
        migrations/010_cash_closures.sql
        migrations/011_batches.sql
        migrations/012_quotes.sql
        migrations/013_purchase_orders.sql
        migrations/014_supplier_details.sql
        migrations/015_credit_notes.sql
        migrations/016_supplier_products.sql
        migrations/017_supplier_payments.sql
        migrations/018_credit_note_lines.sql
        migrations/019_batch_treatments.sql
        migrations/020_reminders.sql
)
