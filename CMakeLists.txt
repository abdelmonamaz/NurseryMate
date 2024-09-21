cmake_minimum_required(VERSION 3.16)

# ── Version via tag git v* (convention GestionScolaire) ────────
find_package(Git QUIET)
set(APP_VERSION "0.1.0")
if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --match "v*" --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE GIT_RESULT
    )
    if(GIT_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        set(APP_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
    endif()
endif()
message(STATUS "Nursera (Pepiniere Ideale) version: ${APP_VERSION}")

project(Nursera VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Quick QuickControls2 Sql Test LinguistTools Multimedia)

# Politiques Qt 6 recommandées (comme GestionScolaire)
qt_policy(SET QTP0001 NEW) # Préfixe /qt/qml/
qt_policy(SET QTP0004 NEW) # qmldir par dossier de module

# Sources UTF-8 sous MSVC (garde de portabilité — MinGW est déjà en UTF-8)
if(MSVC)
    add_compile_options(/utf-8)
endif()

enable_testing()

add_subdirectory(libs/core)
add_subdirectory(libs/services)
add_subdirectory(libs/ui-common)
# Sync mobile (M09) : serveur LAN (desktop) + client/base locale (mobile)
add_subdirectory(libs/sync)
add_subdirectory(apps/desktop)
add_subdirectory(apps/mobile)

if(NOT ANDROID)
    add_subdirectory(tests/unit)
endif()
