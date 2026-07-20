cmake_minimum_required(VERSION 3.21)
project(emp-zk VERSION 1.0.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not allowed. Use a separate build directory:\n"
        "  cmake -B build && cmake --build build")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        Debug Release RelWithDebInfo MinSizeRel)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------
option(EMP_ZK_BUILD_TESTS "Build emp-zk tests"            ${PROJECT_IS_TOP_LEVEL})
option(EMP_ZK_BUILD_BENCHMARKS "Build emp-zk benchmarks (build-only, not ctest)" OFF)
option(EMP_ZK_INSTALL     "Generate install / export rules" ${PROJECT_IS_TOP_LEVEL})

# ---------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------
# Local-dev tip: point at sibling build trees with
#   -Demp-tool_DIR=/path/to/emp-tool/build
#   -Demp-ot_DIR=/path/to/emp-ot/build
# emp-ot's package config already pulls in emp-tool::emp-tool transitively.
find_package(emp-ot 1.0 REQUIRED)

# ---------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------
add_library(emp-zk STATIC
    emp-zk/emp-zk-arith/emp-zk-arith.cpp)
add_library(emp-zk::emp-zk ALIAS emp-zk)

# emp-zk-bool is a native C++20 BooleanContext (ZKBoolContext / ZKBoolSession);
# C++20 is also inherited transitively via emp-ot/emp-tool. Declared here for an
# honest export contract.
target_compile_features(emp-zk PUBLIC cxx_std_20)

target_include_directories(emp-zk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_link_libraries(emp-zk PUBLIC emp-ot::emp-ot)

# ---------------------------------------------------------------------
# Install + export
# ---------------------------------------------------------------------
if(EMP_ZK_INSTALL)
    set(_cmake_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/emp-zk)

    install(TARGETS emp-zk EXPORT emp-zkTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(DIRECTORY emp-zk
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING
            PATTERN "*.h")

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/emp-zk)

    install(EXPORT emp-zkTargets
        NAMESPACE   emp-zk::
        DESTINATION ${_cmake_install_dir}
        FILE        emp-zkTargets.cmake)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/emp-zk-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/emp-zk-config.cmake
        INSTALL_DESTINATION ${_cmake_install_dir})

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/emp-zk-config-version.cmake
        VERSION       ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/emp-zk-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/emp-zk-config-version.cmake
        DESTINATION ${_cmake_install_dir})
endif()

# Build-tree export so siblings can consume without `make install`.
export(EXPORT emp-zkTargets
       NAMESPACE emp-zk::
       FILE      ${CMAKE_CURRENT_BINARY_DIR}/emp-zkTargets.cmake)

# ---------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------
if(EMP_ZK_BUILD_TESTS)
    include(CTest)
    add_subdirectory(test)
endif()

if(EMP_ZK_BUILD_BENCHMARKS)
    add_subdirectory(bench)   # performance benchmarks (build-only)
endif()
