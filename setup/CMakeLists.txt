# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Private application module, not a public SDK or a second installer backend.
qt_add_library(qor_setup_session STATIC sessionwelcome.h sessionwelcome.cpp)
target_link_libraries(qor_setup_session PUBLIC Qt6::Core PRIVATE qor::compiler_settings)
target_include_directories(qor_setup_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
qt_add_library(qor_setup STATIC)
qt_add_qml_module(qor_setup
    URI QOR.Setup
    VERSION 1.0
    DEPENDENCIES QtQml QtQuick QOR.Kit
    RESOURCE_PREFIX /qt/qml
    QML_FILES Welcome.qml Installer.qml
    SOURCES setupstate.h setupstate.cpp installerstate.h installerstate.cpp)
target_link_libraries(qor_setup PUBLIC Qt6::Quick Qt6::QuickControls2 qor_kit PRIVATE qor::compiler_settings)
target_include_directories(qor_setup PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_dependencies(qor_setup qor_boot_branding_check)

qt_add_executable(qor-setup main.cpp)
target_link_libraries(qor-setup PRIVATE qor_setup qor_setupplugin qor_kitplugin
    qor_lumen_styleplugin Qt6::QuickControls2 qor::compiler_settings)
if(QOR_BUILD_SIMULATOR)
    target_compile_definitions(qor-setup PRIVATE QOR_SETUP_DEVELOPMENT)
endif()
set_target_properties(qor-setup PROPERTIES WIN32_EXECUTABLE OFF)
install(TARGETS qor-setup RUNTIME DESTINATION bin)
if(QOR_BUILD_VM_INSTALLER)
    qt_add_executable(qor-install installer_main.cpp)
    target_link_libraries(qor-install PRIVATE qor_setup qor_setupplugin qor_kitplugin
        qor_lumen_styleplugin Qt6::QuickControls2 qor::compiler_settings)
    install(TARGETS qor-install RUNTIME DESTINATION bin)
    install(FILES ../tools/image/vmguard.py ../tools/image/guest_inventory.py ../tools/image/vm_install_backend.py
        DESTINATION libexec/qor-os)
endif()
