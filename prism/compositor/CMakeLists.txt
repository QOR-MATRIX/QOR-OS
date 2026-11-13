# prism — the QOR OS Wayland compositor (Linux only, experimental).

# WaylandCompositorPrivate: which configures a client has yet to acknowledge
# (WindowManagement.configureAcked), and a surface's input region and subsurfaces
# (pointercapture.cpp), which Qt keeps private. QuickPrivate: the same file stops Qt Quick's
# frame-synchronous hover while a client has the pointer, and reads the scene in the order
# it is drawn to see what of the shell's lies over a window.
find_package(Qt6 ${QOR_QT_MIN_VERSION} QUIET COMPONENTS
    WaylandCompositor WaylandCompositorPrivate WaylandScannerTools QuickPrivate)
if(NOT Qt6WaylandCompositor_FOUND OR NOT Qt6WaylandCompositorPrivate_FOUND OR NOT Qt6QuickPrivate_FOUND)
    message(WARNING "Qt Wayland Compositor, or Qt Quick's private headers, not found: skipping the prism "
                    "compositor. Install the Qt Wayland Compositor module for your kit to build it.")
    return()
endif()

# wayland-scanner emits the protocol interface tables as C.
enable_language(C)

qt_add_executable(prism main.cpp commandline.h commandline.cpp)

# Protocols Prism implements itself (see extensions.h). The XML is vendored
# from wayland-protocols 1.47; tablet-v2 only provides an interface that
# cursor-shape refers to.
qt6_generate_wayland_protocol_server_sources(prism PRIVATE_CODE FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/fractional-scale-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/cursor-shape-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/tablet-v2.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/xdg-activation-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/relative-pointer-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/protocols/pointer-constraints-unstable-v1.xml
)
# Scanner output is not ours to fix; keep its warnings out of the build log.
foreach(protocol IN ITEMS fractional-scale-v1 cursor-shape-v1 tablet-v2 xdg-activation-v1
                         relative-pointer-unstable-v1 pointer-constraints-unstable-v1)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/qwayland-server-${protocol}.cpp"
                                "${CMAKE_CURRENT_BINARY_DIR}/wayland-${protocol}-protocol.c"
                                PROPERTIES COMPILE_OPTIONS "-w")
endforeach()

qt_add_qml_module(prism
    URI QOR.Prism
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        Compositor.qml
    SOURCES
        extensions.h extensions.cpp
        windowmanagement.h windowmanagement.cpp
        pointercapture.h pointercapture.cpp
    DEPENDENCIES
        QtQuick
        QtWayland.Compositor
        QOR.Kit
        QOR.Shell
)

target_include_directories(prism PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(prism PRIVATE
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WaylandCompositor
    Qt6::WaylandCompositorPrivate
    Qt6::QuickPrivate
    qor_kit
    qor_services
    qor_kitplugin
    qor_lumen_styleplugin
    qor_servicesplugin
    qor_appsplugin
    qor_inputplugin
    qor_storeplugin
    qor_shell
    qor_shellplugin
    qor_audio
    qor::compiler_settings
)
if(QOR_BUILD_SIMULATOR)
    target_link_libraries(prism PRIVATE qor_sim)
    target_compile_definitions(prism PRIVATE QOR_HAS_SIMULATOR)
endif()

install(TARGETS prism RUNTIME DESTINATION bin)
target_link_libraries(prism PRIVATE qor_setup_session)
