# nursera-desktop : gestion & caisse (doc 00 §5)

qt_add_executable(nursera-desktop WIN32
    main.cpp
)

qt_add_qml_module(nursera-desktop
    URI Nursera.Desktop
    VERSION 1.0
    QML_FILES
        Main.qml
        AppHeader.qml
        AppSidebar.qml
        HomePage.qml
        LoginPage.qml
        SalesPage.qml
        CatalogPage.qml
        CustomersPage.qml
        SuppliersPage.qml
        AdminPage.qml
        BatchesPage.qml
        QuotesPage.qml
        ReportsPage.qml
        StockPage.qml
        InventoryPage.qml
        VariantPicker.qml
)

# Assets partagés : logo + polices Inter (reprises de GestionScolaire)
qt_add_resources(nursera-desktop "assets"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/logo.jpeg"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Medium.ttf"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Bold.ttf"
)

# Traductions (source = français ; cible = arabe) — doc 02 §6
qt_add_translations(nursera-desktop
    TS_FILES ${CMAKE_SOURCE_DIR}/i18n/nursera_ar.ts
)

target_compile_definitions(nursera-desktop PRIVATE
    APP_VERSION_STR="${APP_VERSION}"
)

target_link_libraries(nursera-desktop PRIVATE
    nursera_core
    nursera_services
    nursera_ui
    nursera_uiplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

# Serveur LAN de sync mobile (M09) — si Qt HttpServer est disponible
if(TARGET nursera_sync)
    target_link_libraries(nursera-desktop PRIVATE nursera_sync)
    target_compile_definitions(nursera-desktop PRIVATE NURSERA_HAS_SYNC)
endif()
