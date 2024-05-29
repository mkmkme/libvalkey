cmake_minimum_required(VERSION 3.11)
project(valkeycluster)
include(GNUInstallDirs)

# Options
option(ENABLE_SSL        "Enable SSL/TLS support" OFF)
option(DISABLE_TESTS     "Disable compilation of test" OFF)
option(ENABLE_IPV6_TESTS "Enable IPv6 tests requiring special prerequisites" OFF)
option(ENABLE_COVERAGE   "Enable test coverage reporting" OFF)

macro(getVersionBit name)
  set(VERSION_REGEX "^#define ${name} (.+)$")
  file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/valkeycluster.h"
    VERSION_BIT REGEX ${VERSION_REGEX})
  string(REGEX REPLACE ${VERSION_REGEX} "\\1" ${name} "${VERSION_BIT}")
endmacro(getVersionBit)

# Get version information from src
getVersionBit(LIBVALKEYCLUSTER_MAJOR)
getVersionBit(LIBVALKEYCLUSTER_MINOR)
getVersionBit(LIBVALKEYCLUSTER_PATCH)
getVersionBit(LIBVALKEYCLUSTER_SONAME)
set(VERSION "${LIBVALKEYCLUSTER_MAJOR}.${LIBVALKEYCLUSTER_MINOR}.${LIBVALKEYCLUSTER_PATCH}")
message("Detected version: ${VERSION}")

project(valkeycluster
  VERSION "${VERSION}"
  LANGUAGES C)

# Use plain C99 (-std=c99) without extensions like GNU C (-std=gnu99)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)

# Build using a sanitizer
if(USE_SANITIZER)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=${USE_SANITIZER}")
endif()

if(ENABLE_COVERAGE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -O0" )
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage" )
endif()

SET(valkeycluster_sources
    adlist.c
    command.c
    crc16.c
    dict.c
    vkarray.c
    valkeycluster.c
    vkutil.c)

if(WIN32 OR MINGW)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
endif()

add_library(valkeycluster
  SHARED
  ${valkeycluster_sources})

if(NOT MSVC)
  target_compile_options(valkeycluster PRIVATE -Wall -Wextra -pedantic -Werror
    -Wstrict-prototypes -Wwrite-strings -Wno-missing-field-initializers)

  # Add extra defines when CMAKE_BUILD_TYPE is set to Debug
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DVK_ASSERT_PANIC -DVK_HAVE_BACKTRACE")
  # Alternative: -DVK_ASSERT_LOG)
endif()

set_target_properties(valkeycluster
    PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    VERSION "${LIBVALKEYCLUSTER_SONAME}")

find_package(valkey QUIET)
if(NOT valkey_FOUND)
  message("CMake package for 'valkey' not found, searching for the library..")
  find_library(VALKEY_LIB valkey REQUIRED)
  find_path(VALKEY_INCLUDES valkey/valkey.h)
  add_library(valkey::valkey UNKNOWN IMPORTED GLOBAL)
  set_target_properties(valkey::valkey PROPERTIES IMPORTED_LOCATION ${VALKEY_LIB})
  set_target_properties(valkey::valkey PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${VALKEY_INCLUDES})
endif()

target_include_directories(valkeycluster PUBLIC
  $<BUILD_INTERFACE:${valkeys_INCLUDE_DIRS}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>)

if(WIN32 OR MINGW)
  target_link_libraries(valkeycluster PUBLIC ws2_32 valkey::valkey)
else()
  target_link_libraries(valkeycluster PUBLIC valkey::valkey)
endif()

if(ENABLE_SSL)
  find_package(valkey_ssl QUIET)
  if(NOT valkey_ssl_FOUND)
    message("CMake package for 'libvalkey_ssl' not found, searching for the library..")
    find_library(VALKEY_SSL_LIB valkey_ssl REQUIRED)
    find_path(VALKEY_SSL_INCLUDES valkey/valkey_ssl.h)
    add_library(valkey::valkey_ssl UNKNOWN IMPORTED GLOBAL)
    set_target_properties(valkey::valkey_ssl PROPERTIES IMPORTED_LOCATION ${VALKEY_SSL_LIB})
    set_target_properties(valkey::valkey_ssl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${VALKEY_SSL_INCLUDES})
  endif()

  add_library(valkeycluster_ssl
    SHARED valkeycluster_ssl.c)
  set_target_properties(valkeycluster_ssl
    PROPERTIES VERSION "${LIBVALKEYCLUSTER_SONAME}")
  target_link_libraries(valkeycluster_ssl
    PRIVATE valkeycluster
    PUBLIC valkey::valkey_ssl)
endif()

if(NOT DISABLE_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()

# Code formatting target
find_program(CLANG_FORMAT "clang-format")
file(GLOB_RECURSE FILES_TO_FORMAT
  ${PROJECT_SOURCE_DIR}/*.[ch]
)
add_custom_target(format
  COMMAND ${CLANG_FORMAT} -i ${FILES_TO_FORMAT}
)

# Code coverage target
if(ENABLE_COVERAGE)
  find_program(GCOVR "gcovr")

  add_custom_command(OUTPUT _run_gcovr
    POST_BUILD
    COMMAND ${GCOVR} -r ${CMAKE_SOURCE_DIR} --object-dir=${CMAKE_BINARY_DIR} --html-details coverage.html
    COMMAND echo "Coverage report generated: ${CMAKE_BINARY_DIR}/coverage.html"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_custom_target (coverage DEPENDS _run_gcovr)
endif()

configure_file(valkeycluster.pc.in valkeycluster.pc @ONLY)

install(TARGETS valkeycluster
  EXPORT valkeycluster-targets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES valkeycluster.h adlist.h vkarray.h dict.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/valkeycluster)

install(DIRECTORY adapters
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/valkeycluster)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster.pc
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

export(EXPORT valkeycluster-targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster-targets.cmake
  NAMESPACE valkeycluster::)

set(CMAKE_CONF_INSTALL_DIR share/valkeycluster)
set(INCLUDE_INSTALL_DIR include)
include(CMakePackageConfigHelpers)
configure_package_config_file(valkeycluster-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster-config.cmake
  INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
  PATH_VARS INCLUDE_INSTALL_DIR)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/valkeycluster-config-version.cmake
  COMPATIBILITY SameMinorVersion)

install(EXPORT valkeycluster-targets
  FILE valkeycluster-targets.cmake
  NAMESPACE valkeycluster::
  DESTINATION ${CMAKE_CONF_INSTALL_DIR})

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster-config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster-config-version.cmake
  DESTINATION ${CMAKE_CONF_INSTALL_DIR})

# Install target for valkeycluster_ssl
if(ENABLE_SSL)
  configure_file(valkeycluster_ssl.pc.in valkeycluster_ssl.pc @ONLY)

  install(TARGETS valkeycluster_ssl
    EXPORT valkeycluster_ssl-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(FILES valkeycluster_ssl.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/valkeycluster)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster_ssl.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  export(EXPORT valkeycluster_ssl-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster_ssl-targets.cmake
    NAMESPACE valkeycluster::)
  set(CMAKE_CONF_INSTALL_DIR share/valkeycluster_ssl)
  configure_package_config_file(valkeycluster_ssl-config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster_ssl-config.cmake
    INSTALL_DESTINATION ${CMAKE_CONF_INSTALL_DIR}
    PATH_VARS INCLUDE_INSTALL_DIR)
  install(EXPORT valkeycluster_ssl-targets
    FILE valkeycluster_ssl-targets.cmake
    NAMESPACE valkeycluster::
    DESTINATION ${CMAKE_CONF_INSTALL_DIR})
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/valkeycluster_ssl-config.cmake
    DESTINATION ${CMAKE_CONF_INSTALL_DIR})
endif()
