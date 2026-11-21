# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Independent executable: do not configure the simulated shell or core here.
cmake_minimum_required(VERSION 3.27)
project(qor-qonsole LANGUAGES CXX)

option(QOR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(QOR_QONSOLE_VERIFICATION "Compile local-only executable verification controls" OFF)
option(QOR_QONSOLE_BUILD_TESTS "Build isolated controller regression fixtures" OFF)
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/QorCompilerSettings.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/QorHardening.cmake")
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(qor-qonsole WIN32 main.cpp)
target_link_libraries(qor-qonsole PRIVATE Qt6::Quick qor::compiler_settings)
if(WIN32)
    target_sources(qor-qonsole PRIVATE controlleractions.h controllerinput_windows.h controllerinput_windows.cpp
        ../../input/src/playstation.cpp)
    target_include_directories(qor-qonsole PRIVATE ../../input/src)
    target_link_libraries(qor-qonsole PRIVATE user32)
endif()
qt_add_qml_module(qor-qonsole
    URI QOR.Qonsole
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/QOR/Qonsole"
    NO_CACHEGEN
    QML_FILES Main.qml Launcher.qml
)

if(QOR_QONSOLE_VERIFICATION)
    target_sources(qor-qonsole PRIVATE
        ../../tests/qonsole/executable_verification.cpp
        ../../tests/qonsole/executable_verification.h)
    target_include_directories(qor-qonsole PRIVATE ../../tests/qonsole)
    target_compile_definitions(qor-qonsole PRIVATE QOR_QONSOLE_VERIFICATION)
endif()
if(QOR_QONSOLE_BUILD_TESTS)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    qt_add_executable(tst_qonsole_controller ../../tests/qonsole/tst_controller.cpp
        ../../input/src/playstation.cpp)
    target_include_directories(tst_qonsole_controller PRIVATE . ../../input/src)
    target_compile_definitions(tst_qonsole_controller PRIVATE QOR_QONSOLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(tst_qonsole_controller PRIVATE Qt6::Quick Qt6::Test qor::compiler_settings)
endif()
# No install/deploy rule: this target establishes local executable evidence only.
