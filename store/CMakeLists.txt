# QOR.Store — the QOR OS Store client (docs/STORE.md).
#
# The store's catalogue is signed with the store key, and every package's
# manifest with its developer's key; both are verified with the platform's
# crypto (CNG on Windows, OpenSSL elsewhere) before anything is installed.
# catalog/ holds the signed first-party catalogue built into QOR OS, made from
# content/ by tools/store/build_store.py.

set_source_files_properties(qml/Themes.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
    QT_RESOURCE_ALIAS Themes.qml
)
set_source_files_properties(qml/ThemePreview.qml PROPERTIES QT_RESOURCE_ALIAS ThemePreview.qml)

file(GLOB QOR_STORE_PACKAGES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/catalog/packages/*.qorpkg")

qt_add_library(qor_store STATIC)

qt_add_qml_module(qor_store
    URI QOR.Store
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Themes.qml
        qml/ThemePreview.qml
    SOURCES
        qml/store.h qml/store.cpp
        qml/storefronts.h qml/storefronts.cpp
        src/archive.h src/archive.cpp
        src/catalog.h src/catalog.cpp
        src/filerules.h src/filerules.cpp
        src/package.h src/package.cpp
        src/signature.h
        src/storeengine.h src/storeengine.cpp
    RESOURCES
        keys/store.pub
        catalog/catalog.json
        catalog/catalog.json.sig
        ${QOR_STORE_PACKAGES}
    DEPENDENCIES
        QtQuick
        QOR.Kit
        QOR.Services
)

# The demonstration's key, in a build that asked for it and in no other (docs/STORE.md).
if(QOR_STORE_DEMONSTRATION)
    set(QOR_STORE_DEMONSTRATION_PUB "${CMAKE_SOURCE_DIR}/tests/store/fixtures-demo/store.pub")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QOR_STORE_DEMONSTRATION_PUB}")
    file(READ "${QOR_STORE_DEMONSTRATION_PUB}" QOR_STORE_DEMONSTRATION_KEY)
    string(STRIP "${QOR_STORE_DEMONSTRATION_KEY}" QOR_STORE_DEMONSTRATION_KEY)
    set_property(SOURCE qml/storefronts.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "QOR_STORE_DEMONSTRATION_KEY=\"${QOR_STORE_DEMONSTRATION_KEY}\"")
    # The engines this system has, for a listing's `engines` to be checked against: QQ, at the version in this tree. QQ keeps
    # its own build, so this says which QQ this is, and not that it has been built.
    set(QOR_STORE_QQ_PROJECT "${CMAKE_SOURCE_DIR}/products/qq/CMakeLists.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QOR_STORE_QQ_PROJECT}")
    file(STRINGS "${QOR_STORE_QQ_PROJECT}" QOR_STORE_QQ_LINE REGEX "^project\\(QQ VERSION [0-9]+\\.[0-9]+\\.[0-9]+")
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" QOR_STORE_QQ_VERSION "${QOR_STORE_QQ_LINE}")
    if(NOT QOR_STORE_QQ_VERSION)
        message(FATAL_ERROR "QQ's version was not found in ${QOR_STORE_QQ_PROJECT}")
    endif()
    set_property(SOURCE qml/storefronts.cpp APPEND PROPERTY
        COMPILE_DEFINITIONS "QOR_STORE_QQ_VERSION=\"${QOR_STORE_QQ_VERSION}\"")
endif()

if(WIN32)
    target_sources(qor_store PRIVATE src/signature_windows.cpp)
    target_link_libraries(qor_store PRIVATE bcrypt)
else()
    target_sources(qor_store PRIVATE src/signature_openssl.cpp)
    target_link_libraries(qor_store PRIVATE OpenSSL::Crypto)
endif()

target_include_directories(qor_store PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(qor_store
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        qor_kit
        qor_services
    PRIVATE
        Qt6::GuiPrivate
        qor::compiler_settings
)
