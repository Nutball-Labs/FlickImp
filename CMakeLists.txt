cmake_minimum_required(VERSION 3.16)

# --- Read version from lib/version.hpp (single source of truth) -----------
# Bump the version only in version.hpp — CMakeLists.txt and CPack follow.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/lib/version.hpp" _ver_lines
     REGEX "#define VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")
foreach(_line ${_ver_lines})
    if(_line MATCHES "#define VERSION_MAJOR[ \t]+([0-9]+)")
        set(_vmaj ${CMAKE_MATCH_1})
    endif()
    if(_line MATCHES "#define VERSION_MINOR[ \t]+([0-9]+)")
        set(_vmin ${CMAKE_MATCH_1})
    endif()
    if(_line MATCHES "#define VERSION_PATCH[ \t]+([0-9]+)")
        set(_vpat ${CMAKE_MATCH_1})
    endif()
endforeach()
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/lib/version.hpp" _suf_line
     REGEX "#define VERSION_SUFFIX")
if(_suf_line MATCHES "#define VERSION_SUFFIX[ \t]+\"([^\"]*)\"")
    set(_vsuf ${CMAKE_MATCH_1})
else()
    set(_vsuf "")
endif()

project(FlickImp VERSION ${_vmaj}.${_vmin}.${_vpat} LANGUAGES CXX C)

# Cache var lets build/package scripts detect version changes and reconfigure
# Includes suffix (e.g. "a", "b", "rc1") so cache-busting works correctly.
set(FLICKIMP_VERSION "${PROJECT_VERSION}${_vsuf}" CACHE STRING "FlickImp version for cache-busting" FORCE)

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

# --- libcurl (system / vcpkg) --------------------------------------------
find_package(CURL REQUIRED)

# --- Platform-specific settings ------------------------------------------
if(WIN32)
    # Tell curl headers we're linking statically (vcpkg x64-windows-static)
    add_compile_definitions(CURL_STATICLIB)
    # httplib needs Winsock2; static curl needs Windows LDAP and crypto libs
    set(PLATFORM_LIBS ws2_32 wldap32 crypt32 normaliz)
else()
    set(PLATFORM_LIBS "")
endif()

# --- flickimpcorelib (platform + config — no SQLite/curl dep) ------------
# Linked by both the daemon and the Qt configurator.
add_library(flickimpcorelib STATIC
    lib/platform.cpp
    lib/config.cpp
)
target_include_directories(flickimpcorelib PUBLIC lib third_party)

# --- flickimplib (full library: DB + scraper) -----------------------------
add_library(flickimplib STATIC
    lib/database.cpp
    lib/scraper.cpp
)
target_include_directories(flickimplib PUBLIC lib)
target_link_libraries(flickimplib
    PUBLIC  sqlite3_bundled flickimpcorelib
    PRIVATE CURL::libcurl nlohmann_json ${PLATFORM_LIBS}
)

# --- flickimp (service daemon) --------------------------------------------
add_executable(flickimp
    service/main.cpp
    service/server.cpp
)
target_link_libraries(flickimp
    PRIVATE flickimplib httplib nlohmann_json ${PLATFORM_LIBS}
)
# Embed the installed data dir so the daemon can find web assets after install
# Copy web assets to build dir for running in-place during development
add_custom_command(TARGET flickimp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/web"
        "$<TARGET_FILE_DIR:flickimp>/web"
    COMMENT "Copying web assets to build dir"
)

# --- flickimp-config (Qt6 service configurator — Linux only, optional) ---
if(UNIX AND NOT APPLE)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        qt_add_executable(flickimp-config
            gui/main.cpp
            gui/MainWindow.h
            gui/MainWindow.cpp
        )
        set_target_properties(flickimp-config PROPERTIES AUTOMOC ON)
        target_include_directories(flickimp-config PRIVATE lib third_party gui)
        target_link_libraries(flickimp-config
            PRIVATE Qt6::Widgets flickimpcorelib
        )
    else()
        message(STATUS "Qt6 Widgets not found — flickimp-config will not be built")
        message(STATUS "  Install: sudo dnf install qt6-qtbase-devel")
    endif()
endif()

# --- Install --------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS flickimp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY web/ DESTINATION /var/lib/flickimp/web)
if(UNIX AND NOT APPLE)
    if(Qt6_FOUND)
        install(TARGETS flickimp-config RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(FILES service/flickimp.service
            DESTINATION lib/systemd/system)
    install(PROGRAMS scripts/install-service.sh
            DESTINATION ${CMAKE_INSTALL_DATADIR}/flickimp)
    install(FILES service/flickimp.service
            DESTINATION ${CMAKE_INSTALL_DATADIR}/flickimp)
endif()

# --- CPack (package-linux.sh) --------------------------------------------
# Match the cmake install prefix so FLICKIMP_DATADIR and package paths agree.
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_SET_DESTDIR             ON)
set(CPACK_PACKAGE_NAME            "flickimp")
set(CPACK_PACKAGE_VERSION         "${PROJECT_VERSION}${_vsuf}")
set(CPACK_PACKAGE_CONTACT         "Nutball Labs <claudeai@iceberg.email>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FlickImp — self-hosted watchlist tracker")
set(CPACK_PACKAGE_DESCRIPTION
    "A self-hosted web app for tracking TV shows and movies.\n"
    "Runs as a lightweight HTTP daemon; browser UI; IMDB integration.")
set(CPACK_GENERATOR               "RPM;DEB;TGZ")
# RPM-specific
set(CPACK_RPM_PACKAGE_LICENSE     "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP       "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES    "libcurl")
set(CPACK_RPM_PACKAGE_RELEASE     "1")
set(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE
    "${CMAKE_SOURCE_DIR}/cmake/rpm_pre_install.sh")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE
    "${CMAKE_SOURCE_DIR}/cmake/rpm_post_install.sh")
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE
    "${CMAKE_SOURCE_DIR}/cmake/rpm_preun.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE
    "${CMAKE_SOURCE_DIR}/cmake/rpm_postun.sh")
# DEB-specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER  "Nutball Labs <claudeai@iceberg.email>")
set(CPACK_DEBIAN_PACKAGE_SECTION     "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY    "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS     "libcurl4 | libcurl3")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE    "https://github.com/Nutball-Labs/FlickImp")
include(CPack)

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

# SN: 00003
