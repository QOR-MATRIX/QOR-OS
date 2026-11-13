# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Build-host asset compiler; never part of the installed device session.
qt_add_executable(qor-brand-assets main.cpp)
qt_add_resources(qor-brand-assets brand_mark PREFIX "/qor/branding" FILES Mark.qml)
target_link_libraries(qor-brand-assets PRIVATE
    Qt6::Gui Qt6::Quick qor_kit qor_kitplugin qor::compiler_settings)
target_compile_definitions(qor-brand-assets PRIVATE QOR_BRAND_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_target_properties(qor-brand-assets PROPERTIES WIN32_EXECUTABLE OFF)
add_custom_target(qor_brand_assets_qmllint
    COMMAND $<TARGET_FILE:Qt6::qmllint> -I "${CMAKE_BINARY_DIR}/qml" "${CMAKE_CURRENT_SOURCE_DIR}/Mark.qml"
    DEPENDS qor_kit_qmltyperegistration
    VERBATIM)
if(TARGET all_qmllint)
    add_dependencies(all_qmllint qor_brand_assets_qmllint)
endif()
