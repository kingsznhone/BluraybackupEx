cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" BLURAYBACKUP_VERSION
     LIMIT_COUNT 1)
if(NOT BLURAYBACKUP_VERSION MATCHES "^[0-9]+\.[0-9]+\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a semantic version like 2.1.0")
endif()

set(BLURAYBACKUP_EX_NAME "bluraybackup-ex")

project(${BLURAYBACKUP_EX_NAME} VERSION ${BLURAYBACKUP_VERSION} LANGUAGES C)

# ---- C standard (global) ----
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---- libbluray ----
# Linux: pkg-config  |  Windows (vcpkg): find_path / find_library
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(libbluray REQUIRED IMPORTED_TARGET libbluray)
    pkg_check_modules(libudfread REQUIRED IMPORTED_TARGET libudfread)
    set(_LIBBLURAY_TARGET PkgConfig::libbluray PkgConfig::libudfread)
else()
    find_path(LIBBLURAY_INCLUDE_DIR NAMES libbluray/bluray.h REQUIRED)
    find_library(LIBBLURAY_LIBRARY NAMES bluray REQUIRED)
    find_library(LIBUDFREAD_LIBRARY NAMES udfread REQUIRED)
    add_library(_libbluray_manual INTERFACE)
    target_include_directories(_libbluray_manual INTERFACE ${LIBBLURAY_INCLUDE_DIR})
    target_link_libraries(_libbluray_manual INTERFACE ${LIBBLURAY_LIBRARY} ${LIBUDFREAD_LIBRARY})
    set(_LIBBLURAY_TARGET _libbluray_manual)
endif()

find_package(Threads REQUIRED)

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
    $<$<NOT:$<PLATFORM_ID:Windows>>:_POSIX_C_SOURCE=200809L>
    $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W4 /wd4100 /wd4820 /utf-8>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${_LIBBLURAY_TARGET} Threads::Threads)

# Install rule (used on Linux/macOS)
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PROJECT_NAME}.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# vcpkg's VCPKG_APPLOCAL_DEPS (set ON in CMakePresets.json) handles copying
# all required DLLs, including transitive dependencies, to the output directory.
