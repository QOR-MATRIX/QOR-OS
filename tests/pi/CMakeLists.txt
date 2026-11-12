# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
cmake_minimum_required(VERSION 3.27)

# Keep the native/static-kit gate even for an explicit -S invocation. CMake
# toolchains run earlier and still require their separate provenance review.
list(APPEND CMAKE_PROJECT_TOP_LEVEL_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake/QorPiNative.cmake")
project(qor-pi-smoke VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/QorCompilerSettings.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/QorHardening.cmake")
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Test)
enable_testing()

# Compile the actual Prism parser and its existing hostile-option tests. These
# guiless tests open no display, device, keyring, socket or user configuration.
# The development variant tests compile-time options; it starts no simulator.
foreach(target IN ITEMS tst_prismoptions tst_prismoptions_device)
    qt_add_executable(${target}
        ../prism/tst_prismoptions.cpp
        ../../prism/compositor/commandline.cpp
        ../../prism/compositor/commandline.h)
    target_include_directories(${target} PRIVATE ../../prism/compositor)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Test qor::compiler_settings)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 30
        ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")
endforeach()
target_compile_definitions(tst_prismoptions PRIVATE QOR_HAS_SIMULATOR)
