# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Qt 6.12.0's SpatialAudio tooling metadata omits the module owning QQuick3DNode.
# Correct a build-local copy for QML tools; never edit or install over the SDK.
include_guard(GLOBAL)

function(qor_spatial_audio_metadata_import_path out_var)
    set(${out_var} "" PARENT_SCOPE)
    set(source "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}/QtQuick3D/SpatialAudio")
    if(NOT EXISTS "${source}/qmldir" OR NOT EXISTS "${source}/plugins.qmltypes")
        message(FATAL_ERROR "The selected Qt kit lacks SpatialAudio tooling metadata")
    endif()
    file(READ "${source}/qmldir" module)
    if(module MATCHES "(^|\n)depends[ \t]+QtQuick3D([ \t\r\n]|$)")
        return()
    endif()
    file(READ "${source}/plugins.qmltypes" types)
    if(NOT Qt6_VERSION VERSION_EQUAL "6.12.0"
       OR NOT module MATCHES "(^|\n)module QtQuick3D[.]SpatialAudio[\r\n]"
       OR NOT types MATCHES "prototype: \"QQuick3DNode\"")
        message(FATAL_ERROR "Review the SpatialAudio metadata before applying the Qt 6.12.0 dependency correction")
    endif()

    set(import_root "${CMAKE_CURRENT_BINARY_DIR}/qor-qml-tooling")
    set(destination "${import_root}/QtQuick3D/SpatialAudio")
    file(MAKE_DIRECTORY "${destination}")
    # configure_file and file(CONFIGURE) avoid rewriting identical outputs.
    configure_file("${source}/plugins.qmltypes" "${destination}/plugins.qmltypes" COPYONLY)
    file(CONFIGURE OUTPUT "${destination}/qmldir"
        CONTENT "${module}\ndepends QtQuick3D\n" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}/qmldir")
    set(${out_var} "${import_root}" PARENT_SCOPE)
endfunction()
