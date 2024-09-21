# nursera-mobile : compagnon terrain Android, offline-first (doc 00 §5, M09)
# Compile aussi en desktop pour la prévisualisation rapide.

# ZXing-cpp (Apache 2.0) : décodage QR caméra pour l'appairage (F09-09 bis).
# Lecture seule (qrcodegen reste chargé de la génération côté desktop) —
# writers/exemples/tests désactivés pour ne pas alourdir le build mobile.
include(FetchContent)
set(ZXING_READERS ON CACHE BOOL "" FORCE)
set(ZXING_WRITERS OFF CACHE STRING "" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZXING_BLACKBOX_TESTS OFF CACHE BOOL "" FORCE)
set(ZXING_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zxing_cpp
    GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
    GIT_TAG v2.3.0
)
FetchContent_MakeAvailable(zxing_cpp)

qt_add_executable(nursera-mobile
    main.cpp
    qr_scanner.h
    qr_scanner.cpp
)

qt_add_qml_module(nursera-mobile
    URI Nursera.Mobile
    VERSION 1.0
    QML_FILES
        Main.qml
        LoginPage.qml
        SearchPage.qml
        MovePage.qml
        InventoryPage.qml
        ScanPage.qml
)

qt_add_resources(nursera-mobile "assets"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/logo.jpeg"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Regular.ttf"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Medium.ttf"
        "${CMAKE_SOURCE_DIR}/fonts/Inter-Bold.ttf"
)

target_compile_definitions(nursera-mobile PRIVATE
    APP_VERSION_STR="${APP_VERSION}"
)

target_link_libraries(nursera-mobile PRIVATE
    nursera_core
    nursera_sync
    nursera_ui
    nursera_uiplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Multimedia
    ZXing::ZXing
)
