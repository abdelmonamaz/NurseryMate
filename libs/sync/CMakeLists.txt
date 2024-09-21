# nursera_sync : serveur LAN de synchronisation mobile (M09, doc 02 §4-5).
# Embarqué dans l'app desktop du poste principal — pas de serveur séparé.
# HTTP/1.1 minimal sur QTcpServer : ne requiert PAS le module Qt HttpServer.

qt_add_library(nursera_sync STATIC
    src/sync_server.h
    src/sync_server.cpp
    src/mobile_store.h
    src/mobile_store.cpp
    src/sync_client.h
    src/sync_client.cpp
)

target_include_directories(nursera_sync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nursera_sync
    PUBLIC nursera_core Qt6::Core Qt6::Network Qt6::Sql)
