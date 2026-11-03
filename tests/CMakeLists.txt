# QOR OS tests. Run with `ctest --preset <preset>`.

find_package(Qt6 ${QOR_QT_MIN_VERSION} REQUIRED COMPONENTS Test QuickTest)

# QtTest logs through Qt's message handler; on Windows that falls back to
# OutputDebugString when stderr is not a console (as under ctest). Tests never
# make sound (the audio engine renders in real time to its null output), and
# only virtual game controllers exist.
set(QOR_TEST_ENV "QT_FORCE_STDERR_LOGGING=1;QOR_AUDIO_OUTPUT=null;QOR_GAMEPADS=virtual;QOR_TOUCHPAD=off")

# ── C++ unit tests ───────────────────────────────────────────────────────────
function(qor_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})
    qt_add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBRARIES} qor::compiler_settings)
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${QOR_TEST_ENV}")
endfunction()

qor_add_test(tst_spring SOURCES kit/tst_spring.cpp LIBRARIES qor_kit)
qor_add_test(tst_oklch SOURCES kit/tst_oklch.cpp LIBRARIES qor_kit)
qor_add_test(tst_services SOURCES services/tst_services.cpp LIBRARIES qor_services qor_sim)
qor_add_test(tst_audio SOURCES audio/tst_audio.cpp LIBRARIES qor_audio)
qor_add_test(tst_input SOURCES input/tst_input.cpp LIBRARIES qor_input qor_kit qor_kitplugin Qt6::GuiPrivate)
qor_add_test(tst_store SOURCES store/tst_store.cpp LIBRARIES qor_store qor_storeplugin Qt6::GuiPrivate)
target_compile_definitions(tst_store PRIVATE
    QOR_STORE_CATALOG="${CMAKE_SOURCE_DIR}/store/catalog"
    QOR_STORE_PUB="${CMAKE_SOURCE_DIR}/store/keys/store.pub"
    QOR_STORE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures")
# Format 2 and every hostile package: it signs what it needs as it runs, with keys it makes and drops (testsigner.h).
if(WIN32)
    set(QOR_STORE_TEST_SIGNER store/testsigner_windows.cpp)
    set(QOR_STORE_TEST_CRYPTO bcrypt)
else()
    set(QOR_STORE_TEST_SIGNER store/testsigner_openssl.cpp)
    set(QOR_STORE_TEST_CRYPTO OpenSSL::Crypto)
endif()
qor_add_test(tst_store_format2
    SOURCES store/tst_store_format2.cpp store/testsigner.h ${QOR_STORE_TEST_SIGNER}
    LIBRARIES qor_store qor_storeplugin Qt6::GuiPrivate ${QOR_STORE_TEST_CRYPTO})
target_compile_definitions(tst_store_format2 PRIVATE
    QOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    QOR_STORE_CATALOG="${CMAKE_SOURCE_DIR}/store/catalog"
    QOR_STORE_PUB="${CMAKE_SOURCE_DIR}/store/keys/store.pub"
    QOR_STORE_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures"
    QOR_STORE_DEMO="${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures-demo"
    QOR_STORE_LICENCES="${CMAKE_CURRENT_SOURCE_DIR}/store/licences")
# What the shell's pages have of the two storefronts, the build choice that keeps them out of everything but a
# demonstration, and what a catalogue hands them, from the hostile side (store/qml/storefronts.h).
qor_add_test(tst_storefronts
    SOURCES store/tst_storefronts.cpp store/testshop.h store/testsigner.h ${QOR_STORE_TEST_SIGNER}
    LIBRARIES qor_store qor_storeplugin qor_sim qor_service_endpoint Qt6::GuiPrivate ${QOR_STORE_TEST_CRYPTO})
target_compile_definitions(tst_storefronts PRIVATE
    QOR_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    QOR_STORE_CATALOG="${CMAKE_SOURCE_DIR}/store/catalog"
    QOR_STORE_PUB="${CMAKE_SOURCE_DIR}/store/keys/store.pub"
    QOR_STORE_DEMO="${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures-demo"
    QOR_SHELL_PROGRAM="$<TARGET_FILE:qor-shell>")
# It reads what the shell and Prism were linked into, so it needs them built.
add_dependencies(tst_storefronts qor-shell)
if(TARGET prism)
    target_compile_definitions(tst_storefronts PRIVATE QOR_PRISM_PROGRAM="$<TARGET_FILE:prism>")
    add_dependencies(tst_storefronts prism)
endif()
if(QOR_STORE_DEMONSTRATION)
    target_compile_definitions(tst_storefronts PRIVATE QOR_STORE_DEMONSTRATION)
endif()
# The Store's real pages over a catalogue the suite signs. Offscreen and in software, so it needs no display and no card.
qt_add_executable(tst_storefronts_qml
    store/tst_storefronts_qml.cpp store/testshop.h store/testsigner.h ${QOR_STORE_TEST_SIGNER})
target_link_libraries(tst_storefronts_qml PRIVATE
    Qt6::QuickTest Qt6::Test Qt6::QuickControls2 Qt6::GuiPrivate
    qor_kit qor_kitplugin qor_lumen_style qor_lumen_styleplugin qor_services qor_servicesplugin qor_input qor_inputplugin
    qor_store qor_storeplugin qor_apps qor_appsplugin qor_sim qor_service_endpoint ${QOR_STORE_TEST_CRYPTO}
    qor::compiler_settings)
target_compile_definitions(tst_storefronts_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/store")
set_target_properties(tst_storefronts_qml PROPERTIES WIN32_EXECUTABLE OFF)
add_test(NAME tst_storefronts_qml
    COMMAND tst_storefronts_qml -input "${CMAKE_CURRENT_SOURCE_DIR}/store/tst_storefronts.qml")
set_tests_properties(tst_storefronts_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;${QOR_TEST_ENV}")
# Prism's side of the QOR host protocol (prism/shell/producthost.h), against
# a product with no window that tst_producthost_product stands in for.
qt_add_executable(tst_producthost_product prism/producthost_product.cpp)
target_link_libraries(tst_producthost_product PRIVATE Qt6::Core Qt6::Network qor::compiler_settings)
set_target_properties(tst_producthost_product PROPERTIES WIN32_EXECUTABLE OFF)
qor_add_test(tst_producthost SOURCES prism/tst_producthost.cpp LIBRARIES qor_shell Qt6::Network)
target_compile_definitions(tst_producthost PRIVATE QOR_TEST_PRODUCT="$<TARGET_FILE:tst_producthost_product>")
add_dependencies(tst_producthost tst_producthost_product)
# The Library (prism/shell/library.h): the games and media it finds, and what was played.
qor_add_test(tst_library SOURCES prism/tst_library.cpp LIBRARIES qor_shell)
# What a Wayland client's app id can make the shell read.
qor_add_test(tst_desktopentries SOURCES prism/tst_desktopentries.cpp LIBRARIES qor_shell)
# The QOR core through its C bridge, built from demiurge-chain at the pinned commit (core/, QOR-0003).
if(TARGET qor::core)
    qor_add_test(tst_qorcore SOURCES core/tst_qorcore.cpp LIBRARIES qor::core)
endif()

# Store widgets are QML outside any module: lint them against the built ones.
file(GLOB QOR_STORE_WIDGET_QML CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/store/content/widgets/*/*.qml")
add_test(NAME store_widgets_qmllint
    COMMAND $<TARGET_FILE:Qt6::qmllint> -I "${CMAKE_BINARY_DIR}/qml" ${QOR_STORE_WIDGET_QML})

# ── QML tests (Qt Quick Test) ───────────────────────────────────────────────
qt_add_executable(tst_shell shell/tst_shell.cpp)
target_link_libraries(tst_shell PRIVATE Qt6::QuickTest)
target_compile_definitions(tst_shell PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shell")
set_target_properties(tst_shell PROPERTIES WIN32_EXECUTABLE OFF)
add_test(NAME tst_shell COMMAND tst_shell)
set_tests_properties(tst_shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${QOR_TEST_ENV}")

# Kit QML tests render real shaders, so they need a GPU-capable platform.
qt_add_executable(tst_kit_qml kit/tst_kit_qml.cpp)
target_link_libraries(tst_kit_qml PRIVATE Qt6::QuickTest qor_kit qor_kitplugin qor::compiler_settings)
target_compile_definitions(tst_kit_qml PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/kit")
set_target_properties(tst_kit_qml PROPERTIES WIN32_EXECUTABLE OFF)
add_test(NAME tst_kit_qml COMMAND tst_kit_qml -input "${CMAKE_CURRENT_SOURCE_DIR}/kit/tst_kit.qml")
set_tests_properties(tst_kit_qml PROPERTIES LABELS "gpu" ENVIRONMENT "${QOR_TEST_ENV}")

# ── Golden-image tests (GPU) ────────────────────────────────────────────────
# Renders golden/scenes in every variant and compares them perceptually with
# golden/references/. `cmake --build --preset <preset> --target update_golden`
# rewrites the references; see docs/DEVELOPMENT.md.
qt_add_executable(tst_golden golden/tst_golden.cpp)
target_link_libraries(tst_golden PRIVATE
    Qt6::Test Qt6::Quick Qt6::QuickControls2
    qor_kit qor_kitplugin qor_lumen_style qor_lumen_styleplugin qor::compiler_settings)
target_compile_definitions(tst_golden PRIVATE
    QOR_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    QOR_GOLDEN_OUT="${CMAKE_CURRENT_BINARY_DIR}/golden-out")
set_target_properties(tst_golden PROPERTIES WIN32_EXECUTABLE OFF)
add_test(NAME tst_golden COMMAND tst_golden)
set_tests_properties(tst_golden PROPERTIES LABELS "gpu" ENVIRONMENT "${QOR_TEST_ENV}")
add_custom_target(update_golden
    COMMAND ${CMAKE_COMMAND} -E env QOR_GOLDEN_UPDATE=1 ${QOR_TEST_ENV} $<TARGET_FILE:tst_golden>
    DEPENDS tst_golden
    COMMENT "Rewriting golden references in tests/golden/references"
    USES_TERMINAL)

find_package(Python3 3.10 COMPONENTS Interpreter)

# ── Shell end-to-end (GPU) ──────────────────────────────────────────────────
# First-party apps driven through qor-shell --script with synthetic input:
# controls receive clicks, and maximise, snap, minimise, restore, Overview and
# close work. The script asserts on its own ("expect" steps).
add_test(NAME shell_e2e
    COMMAND qor-shell --unlocked --size 1280x800 --script "${CMAKE_CURRENT_SOURCE_DIR}/shell/apps.e2e.json")
set_tests_properties(shell_e2e PROPERTIES LABELS "gpu" TIMEOUT 240 ENVIRONMENT "${QOR_TEST_ENV}")
# A script's failure says how long an item's text is, and never what it says (the "item" expectation in
# prism/shell/shellscript.cpp): the script is meant to fail, and passes only on that failure.
add_test(NAME shell_e2e_quiet
    COMMAND qor-shell --unlocked --size 1280x800 --script "${CMAKE_CURRENT_SOURCE_DIR}/shell/item-quiet.e2e.json")
set_tests_properties(shell_e2e_quiet PROPERTIES LABELS "gpu" TIMEOUT 120 ENVIRONMENT "${QOR_TEST_ENV}"
    PASS_REGULAR_EXPRESSION "text is a text of [0-9]+ characters"
    FAIL_REGULAR_EXPRESSION "Music is on its way")
# The Store in the build that was asked for: a person's whole walk through a demonstration's two storefronts, or in any other
# build, that there are none, and the pages say what they said.
if(QOR_STORE_DEMONSTRATION)
    # Nothing of a demonstration build installs (CMakeLists.txt): asked to, it refuses before it has copied anything.
    add_test(NAME demonstration_does_not_install
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}" --prefix "${CMAKE_CURRENT_BINARY_DIR}/refused-install")
    set_tests_properties(demonstration_does_not_install PROPERTIES
        PASS_REGULAR_EXPRESSION "nothing built with it is installed"
        FAIL_REGULAR_EXPRESSION "Installing:|Up-to-date:")
    add_test(NAME shell_e2e_store
        COMMAND qor-shell --unlocked --size 1280x800 --script "${CMAKE_CURRENT_SOURCE_DIR}/shell/store-demonstration.e2e.json")
    set_tests_properties(shell_e2e_store PROPERTIES LABELS "gpu" TIMEOUT 240 ENVIRONMENT
        "${QOR_TEST_ENV};QOR_STORE_DEMONSTRATION_URL=${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures-demo/v1/catalog.json|${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures-demo/v2/catalog.json")
else()
    add_test(NAME shell_e2e_store
        COMMAND qor-shell --unlocked --size 1280x800 --script "${CMAKE_CURRENT_SOURCE_DIR}/shell/store-ordinary.e2e.json")
    set_tests_properties(shell_e2e_store PROPERTIES LABELS "gpu" TIMEOUT 120 ENVIRONMENT
        "${QOR_TEST_ENV};QOR_STORE_DEMONSTRATION_URL=${CMAKE_CURRENT_SOURCE_DIR}/store/fixtures-demo/v1/catalog.json")
endif()

# ── Prism end-to-end (Linux, GPU) ───────────────────────────────────────────
# Real Wayland clients in a nested Prism, driven by `prism --script`:
# decorations, maximise, snap, client moves, minimise and restore, popups,
# Overview and close. Needs a display (Wayland session or WSLg) and, for the
# client-side decoration checks, weston-terminal.
if(TARGET prism)
    find_package(Qt6 ${QOR_QT_MIN_VERSION} REQUIRED COMPONENTS Widgets)
    qt_add_executable(prism-test-client prism/prism_test_client.cpp)
    target_link_libraries(prism-test-client PRIVATE Qt6::Widgets qor::compiler_settings)
    if(Python3_Interpreter_FOUND)
        add_test(NAME prism_e2e
            COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/dev/prism-e2e.py" --build-dir "${CMAKE_BINARY_DIR}")
        set_tests_properties(prism_e2e PROPERTIES LABELS "gpu" TIMEOUT 180)
    endif()

    # ── Prism hosting the products (Linux, no display) ──────────────────────
    # Stand-ins for ARQADE, QOR Media, QQ Studio and the QQ Player, started by
    # Prism as a person would and laid out by it. Prism runs offscreen and
    # draws in software, so this needs no display and no graphics card, and
    # shows nothing.
    foreach(product IN ITEMS arqade media qq-studio qq-player)
        qt_add_executable(prism-test-${product} prism/prism_test_product.cpp)
        target_link_libraries(prism-test-${product} PRIVATE Qt6::Gui Qt6::Network qor::compiler_settings)
    endforeach()
    target_compile_definitions(prism-test-arqade PRIVATE PRODUCT_ID="arqade" PRODUCT_NAME="ARQADE")
    target_compile_definitions(prism-test-media PRIVATE PRODUCT_ID="qor-media" PRODUCT_NAME="QOR Media")
    target_compile_definitions(prism-test-qq-studio PRIVATE PRODUCT_ID="qq-studio" PRODUCT_NAME="QQ Studio")
    target_compile_definitions(prism-test-qq-player PRIVATE PRODUCT_ID="qq-player" PRODUCT_NAME="QQ Player")
    if(Python3_Interpreter_FOUND)
        add_test(NAME prism_products
            COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/dev/prism-products.py"
                --prism $<TARGET_FILE:prism>
                --arqade $<TARGET_FILE:prism-test-arqade>
                --media $<TARGET_FILE:prism-test-media>
                --qq-studio $<TARGET_FILE:prism-test-qq-studio>
                --qq-player $<TARGET_FILE:prism-test-qq-player>
                --script "${CMAKE_CURRENT_SOURCE_DIR}/prism/products.e2e.json")
        set_tests_properties(prism_products PROPERTIES TIMEOUT 120)

        # What a client says about itself is only a claim: one calling itself
        # Settings, and one that opens a window it never draws, hoping for the
        # keyboard. No display and no graphics card, as above.
        add_test(NAME prism_untrusted
            COMMAND Python3::Interpreter "${CMAKE_SOURCE_DIR}/tools/dev/prism-products.py"
                --prism $<TARGET_FILE:prism>
                --arqade $<TARGET_FILE:prism-test-arqade>
                --media $<TARGET_FILE:prism-test-media>
                --qq-studio $<TARGET_FILE:prism-test-qq-studio>
                --qq-player $<TARGET_FILE:prism-test-qq-player>
                --script "${CMAKE_CURRENT_SOURCE_DIR}/prism/untrusted.e2e.json"
                "--client=$<TARGET_FILE:prism-test-client> --app-id settings --title Impostor"
                "--client=$<TARGET_FILE:prism-test-client> --app-id arqade --title Pretender"
                "--client=$<TARGET_FILE:prism-test-client> --unseen --after 9000 --title Unseen")
        set_tests_properties(prism_untrusted PROPERTIES TIMEOUT 120)
    endif()

    # Pointer lock and relative motion (QOR-0020), from the client's side: raw Wayland
    # clients of a Prism this suite starts, offscreen and in software, and drives with a
    # person's input through `prism --script`. No display and no graphics card.
    find_package(Qt6 ${QOR_QT_MIN_VERSION} REQUIRED COMPONENTS WaylandScannerTools)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(QOR_WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    enable_language(C)
    set(pointer_capture_protocols "")
    foreach(protocol IN ITEMS xdg-shell relative-pointer-unstable-v1 pointer-constraints-unstable-v1)
        set(xml "${CMAKE_SOURCE_DIR}/prism/compositor/protocols/${protocol}.xml")
        set(header "${CMAKE_CURRENT_BINARY_DIR}/wayland-${protocol}-client-protocol.h")
        set(code "${CMAKE_CURRENT_BINARY_DIR}/wayland-${protocol}-protocol.c")
        add_custom_command(OUTPUT "${header}" "${code}"
            COMMAND Wayland::Scanner client-header "${xml}" "${header}"
            COMMAND Wayland::Scanner private-code "${xml}" "${code}"
            DEPENDS "${xml}"
            VERBATIM)
        # Scanner output is not ours to fix.
        set_source_files_properties("${code}" PROPERTIES COMPILE_OPTIONS "-w")
        list(APPEND pointer_capture_protocols "${header}" "${code}")
    endforeach()
    qor_add_test(tst_pointercapture
        SOURCES prism/tst_pointercapture.cpp ${pointer_capture_protocols}
        LIBRARIES PkgConfig::QOR_WAYLAND_CLIENT)
    target_include_directories(tst_pointercapture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(tst_pointercapture PRIVATE QOR_PRISM="$<TARGET_FILE:prism>")
    add_dependencies(tst_pointercapture prism)
    set_tests_properties(tst_pointercapture PROPERTIES TIMEOUT 300)
endif()

# ── Tooling tests (Python) ──────────────────────────────────────────────────
if(Python3_Interpreter_FOUND)
    add_test(NAME tools_generate_tokens
        COMMAND Python3::Interpreter -m unittest -v test_generate_tokens
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tools"
    )
endif()
