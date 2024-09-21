qt_add_executable(tst_money tst_money.cpp)
target_link_libraries(tst_money PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_money COMMAND tst_money)

qt_add_executable(tst_database tst_database.cpp)
target_link_libraries(tst_database PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_database COMMAND tst_database)

qt_add_executable(tst_catalog tst_catalog.cpp)
target_link_libraries(tst_catalog PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_catalog COMMAND tst_catalog)

qt_add_executable(tst_stock tst_stock.cpp)
target_link_libraries(tst_stock PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_stock COMMAND tst_stock)

qt_add_executable(tst_inventory tst_inventory.cpp)
target_link_libraries(tst_inventory PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_inventory COMMAND tst_inventory)

qt_add_executable(tst_auth tst_auth.cpp)
target_link_libraries(tst_auth PRIVATE nursera_core nursera_services Qt6::Test)
add_test(NAME tst_auth COMMAND tst_auth)

qt_add_executable(tst_sales tst_sales.cpp)
target_link_libraries(tst_sales PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_sales COMMAND tst_sales)

qt_add_executable(tst_ticket tst_ticket.cpp)
target_link_libraries(tst_ticket PRIVATE nursera_core nursera_services Qt6::Test)
add_test(NAME tst_ticket COMMAND tst_ticket)

qt_add_executable(tst_dashboard tst_dashboard.cpp)
target_link_libraries(tst_dashboard PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_dashboard COMMAND tst_dashboard)

qt_add_executable(tst_customers tst_customers.cpp)
target_link_libraries(tst_customers PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_customers COMMAND tst_customers)

qt_add_executable(tst_suppliers tst_suppliers.cpp)
target_link_libraries(tst_suppliers PRIVATE nursera_core nursera_services Qt6::Sql Qt6::Test)
add_test(NAME tst_suppliers COMMAND tst_suppliers)

qt_add_executable(tst_admin tst_admin.cpp)
target_link_libraries(tst_admin PRIVATE nursera_core nursera_services Qt6::Test)
add_test(NAME tst_admin COMMAND tst_admin)

qt_add_executable(tst_amount_words tst_amount_words.cpp)
target_link_libraries(tst_amount_words PRIVATE nursera_core Qt6::Test)
add_test(NAME tst_amount_words COMMAND tst_amount_words)

qt_add_executable(tst_invoice tst_invoice.cpp)
target_link_libraries(tst_invoice PRIVATE nursera_core nursera_services Qt6::Sql Qt6::Test)
add_test(NAME tst_invoice COMMAND tst_invoice)

qt_add_executable(tst_pos tst_pos.cpp)
target_link_libraries(tst_pos PRIVATE nursera_core nursera_services Qt6::Test)
add_test(NAME tst_pos COMMAND tst_pos)

qt_add_executable(tst_batches tst_batches.cpp)
target_link_libraries(tst_batches PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_batches COMMAND tst_batches)

qt_add_executable(tst_labels tst_labels.cpp)
target_link_libraries(tst_labels PRIVATE nursera_core nursera_services Qt6::Test)
add_test(NAME tst_labels COMMAND tst_labels)

qt_add_executable(tst_quotes tst_quotes.cpp)
target_link_libraries(tst_quotes PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_quotes COMMAND tst_quotes)

qt_add_executable(tst_reports tst_reports.cpp)
target_link_libraries(tst_reports PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_reports COMMAND tst_reports)

qt_add_executable(tst_purchase_orders tst_purchase_orders.cpp)
target_link_libraries(tst_purchase_orders PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_purchase_orders COMMAND tst_purchase_orders)

qt_add_executable(tst_reminders tst_reminders.cpp)
target_link_libraries(tst_reminders PRIVATE nursera_core Qt6::Sql Qt6::Test)
add_test(NAME tst_reminders COMMAND tst_reminders)

qt_add_executable(tst_report_pdf tst_report_pdf.cpp)
target_link_libraries(tst_report_pdf PRIVATE nursera_core nursera_services Qt6::Sql Qt6::Test)
add_test(NAME tst_report_pdf COMMAND tst_report_pdf)

if(TARGET nursera_sync)
    qt_add_executable(tst_sync tst_sync.cpp)
    target_link_libraries(tst_sync PRIVATE nursera_core nursera_sync
        Qt6::Network Qt6::Sql Qt6::Test)
    add_test(NAME tst_sync COMMAND tst_sync)

    qt_add_executable(tst_mobile_sync tst_mobile_sync.cpp)
    target_link_libraries(tst_mobile_sync PRIVATE nursera_core nursera_sync
        Qt6::Network Qt6::Sql Qt6::Test)
    add_test(NAME tst_mobile_sync COMMAND tst_mobile_sync)
endif()
