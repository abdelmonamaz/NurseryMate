# nursera_services : controllers + modèles de liste QML (pattern GS_Services)

qt_add_library(nursera_services STATIC
    src/qml_models/product_list_model.h
    src/qml_models/product_list_model.cpp
    src/qml_models/stock_list_model.h
    src/qml_models/stock_list_model.cpp
    src/qml_models/inventory_list_model.h
    src/qml_models/inventory_list_model.cpp
    src/qml_models/cart_model.h
    src/qml_models/cart_model.cpp

    src/controllers/admin_controller.h
    src/controllers/admin_controller.cpp
    src/controllers/auth_controller.h
    src/controllers/auth_controller.cpp
    src/controllers/batch_controller.h
    src/controllers/batch_controller.cpp
    src/controllers/catalog_controller.h
    src/controllers/catalog_controller.cpp
    src/controllers/customer_controller.h
    src/controllers/customer_controller.cpp
    src/controllers/dashboard_controller.h
    src/controllers/dashboard_controller.cpp
    src/controllers/stock_controller.h
    src/controllers/stock_controller.cpp
    src/controllers/supplier_controller.h
    src/controllers/supplier_controller.cpp
    src/controllers/inventory_controller.h
    src/controllers/inventory_controller.cpp
    src/controllers/invoice_controller.h
    src/controllers/invoice_controller.cpp
    src/controllers/quote_controller.h
    src/controllers/quote_controller.cpp
    src/controllers/report_controller.h
    src/controllers/report_controller.cpp
    src/controllers/reminder_controller.h
    src/controllers/reminder_controller.cpp
    src/controllers/sale_controller.h
    src/controllers/sale_controller.cpp

    src/generators/ticket_generator.h
    src/generators/ticket_generator.cpp
    src/generators/invoice_generator.h
    src/generators/invoice_generator.cpp
    src/generators/credit_note_generator.h
    src/generators/credit_note_generator.cpp
    src/generators/label_generator.h
    src/generators/label_generator.cpp
    src/generators/quote_generator.h
    src/generators/quote_generator.cpp
    src/generators/report_generator.h
    src/generators/report_generator.cpp

    # QR — qrcodegen (Nayuki, licence MIT) — génération seule
    src/thirdparty/qrcodegen/qrcodegen.hpp
    src/thirdparty/qrcodegen/qrcodegen.cpp
)

target_include_directories(nursera_services PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/thirdparty/qrcodegen)

target_include_directories(nursera_services PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nursera_services
    PUBLIC nursera_core Qt6::Core Qt6::Gui
    PRIVATE Qt6::Network) # IP locales pour l'écran Réglages (sync M09)
