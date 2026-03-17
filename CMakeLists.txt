cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" BLURAYBACKUP_VERSION
     LIMIT_COUNT 1)
if(NOT BLURAYBACKUP_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version like 2.1.0")
endif()

set(BLURAYBACKUP_EX_NAME "bluraybackup-ex")

project(${BLURAYBACKUP_EX_NAME} VERSION ${BLURAYBACKUP_VERSION} LANGUAGES C)

set(_BLURAYBACKUP_LEGACY_INSTALL_PREFIX "C:/Program Files/bluraybackup")

if(WIN32 AND (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX STREQUAL "${_BLURAYBACKUP_LEGACY_INSTALL_PREFIX}"))
    set(CMAKE_INSTALL_PREFIX "C:/Program Files/${PROJECT_NAME}" CACHE PATH "Install path prefix." FORCE)
endif()

# ---- libbluray: prefer pkg-config (Linux/macOS), fall back to manual find (vcpkg/Windows) ----
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBBLURAY IMPORTED_TARGET libbluray)
endif()

if(LIBBLURAY_FOUND)
    # pkg-config path: use the imported target directly
    set(_LIBBLURAY_TARGET PkgConfig::LIBBLURAY)
else()
    # Manual find: used by vcpkg on Windows
    find_path(LIBBLURAY_INCLUDE_DIR
        NAMES libbluray/bluray.h
        REQUIRED
    )
    find_library(LIBBLURAY_LIBRARY
        NAMES bluray
        REQUIRED
    )
    add_library(_libbluray_manual INTERFACE)
    target_include_directories(_libbluray_manual INTERFACE ${LIBBLURAY_INCLUDE_DIR})
    target_link_libraries(_libbluray_manual INTERFACE ${LIBBLURAY_LIBRARY})
    set(_LIBBLURAY_TARGET _libbluray_manual)
endif()

add_executable(${PROJECT_NAME}
    src/main.c
    src/cli.c
    src/bluray.c
    src/async_writer.c
    src/progress.c
)

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    BIN="${PROJECT_NAME}"
    VERSION="${PROJECT_VERSION}"
)

if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _POSIX_C_SOURCE=200809L)
endif()

find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${_LIBBLURAY_TARGET} Threads::Threads)

# Require C17
target_compile_features(${PROJECT_NAME} PRIVATE c_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

# Disable MSVC deprecation warnings for POSIX names on Windows
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /wd4100 /wd4820 /utf-8)
endif()

if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install rule (used on Linux/macOS)
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PROJECT_NAME}.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# vcpkg's VCPKG_APPLOCAL_DEPS (set ON in CMakePresets.json) handles copying
# all required DLLs, including transitive dependencies, to the output directory.
