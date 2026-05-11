cmake_minimum_required(VERSION 3.16)
project(FlickImp VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Packages dir (CPack output) -----------------------------------------
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_SOURCE_DIR}/packages")

# --- Third-party: sqlite3 (amalgamation) ---------------------------------
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite3/sqlite3.c")
    message(FATAL_ERROR
        "SQLite3 amalgamation not found.\n"
        "Run:  ./scripts/get-deps.sh\n"
        "  or: sudo dnf install sqlite-devel  (then remove the bundled path from CMakeLists)")
endif()
add_library(sqlite3_bundled STATIC third_party/sqlite3/sqlite3.c)
target_include_directories(sqlite3_bundled PUBLIC third_party/sqlite3)

# --- Third-party: cpp-httplib (header-only) --------------------------------
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/httplib.h")
    message(FATAL_ERROR "cpp-httplib not found. Run: ./scripts/get-deps.sh")
endif()
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE third_party)

# --- Third-party: nlohmann/json (header-only) -----------------------------
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "nlohmann/json not found. Run: ./scripts/get-deps.sh")
endif()
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE third_party)

# --- libcurl (system) -----------------------------------------------------
find_package(CURL REQUIRED)

# --- watchinglib ----------------------------------------------------------
add_library(watchinglib STATIC
    lib/platform.cpp
    lib/database.cpp
    lib/scraper.cpp
)
target_include_directories(watchinglib PUBLIC lib)
target_link_libraries(watchinglib
    PUBLIC  sqlite3_bundled
    PRIVATE CURL::libcurl nlohmann_json
)

# --- flickimp (service daemon) --------------------------------------------
add_executable(flickimp
    service/main.cpp
    service/server.cpp
)
target_link_libraries(flickimp
    PRIVATE watchinglib httplib nlohmann_json
)

# Copy web assets to build dir for running in-place during development
add_custom_command(TARGET flickimp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:flickimp>/web"
    COMMENT "Copying web assets to build dir"
)

# --- Install --------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS flickimp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY web/ DESTINATION ${CMAKE_INSTALL_DATADIR}/flickimp/web)

# --- SN audit / archive ---------------------------------------------------
add_custom_target(sn-audit
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/sn_audit.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Auditing serial numbers"
)

add_custom_target(archive
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/archive.cmake
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating source archive"
)

# SN: 00001
