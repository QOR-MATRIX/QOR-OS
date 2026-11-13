# QQ's tests: Qt Test, rendering for real on the machine's GPU and judging what was drawn by its pixels.
#
# Changed for QOR OS: every suite is built with virtualpads.cpp, so no controller on the machine joins in.
# Changed for QOR OS: tst_workspace; tst_assetlibrary.
# Changed for QOR OS: tst_storegame, a game from the Store played by the real Player.

qt_add_executable(tst_world tst_world.cpp)

target_compile_definitions(tst_world PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_link_libraries(tst_world PRIVATE
    Qt6::Test Qt6::Gui Qt6::Quick Qt6::Quick3D
    qqruntime qqruntimeplugin
)

add_test(NAME tst_world COMMAND tst_world)

qt_add_executable(tst_scene tst_scene.cpp)

# The one look's source (ADR-108): Look is checked against it, value by value.
target_compile_definitions(tst_scene PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_TOKENS="${CMAKE_CURRENT_SOURCE_DIR}/../../../docs/design/tokens.json")

target_link_libraries(tst_scene PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D
    qqruntime qqruntimeplugin
)

# A model inside the executable, as the Studio bundles its sample: adopting one into a project copies it out.
qt_add_resources(tst_scene "fixtures" PREFIX /fixtures BASE fixtures FILES fixtures/orb.gltf)

add_test(NAME tst_scene COMMAND tst_scene)

qt_add_executable(tst_studio tst_studio.cpp)

target_compile_definitions(tst_studio PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_link_libraries(tst_studio PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2
    qqstudio qqstudioplugin qqruntime qqruntimeplugin
)

add_test(NAME tst_studio COMMAND tst_studio)

qt_add_executable(tst_play tst_play.cpp)

target_compile_definitions(tst_play PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}/../samples")

target_link_libraries(tst_play PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D
    qqruntime qqruntimeplugin
)

add_test(NAME tst_play COMMAND tst_play)

qt_add_executable(tst_player tst_player.cpp)

# The real player, started as the launcher starts it.
target_compile_definitions(tst_player PRIVATE
    QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}/../samples"
    QQ_PLAYER="$<TARGET_FILE:qq-player>")

# The runtime only for where the Player keeps a game's store (P3.16): the check finds it as the Player does.
target_link_libraries(tst_player PRIVATE Qt6::Test Qt6::Gui Qt6::Qml qqruntime)
add_dependencies(tst_player qq-player)

add_test(NAME tst_player COMMAND tst_player)

# Changed for QOR OS: the demonstration's Ember Climb, laid out from its signed package as QOR OS's Store installs it, played
# by the real Player behind its walls from the scene its game.json names. The package is QOR OS's, read and never signed.
qt_add_executable(tst_storegame tst_storegame.cpp)
target_compile_definitions(tst_storegame PRIVATE
    QQ_PLAYER="$<TARGET_FILE:qq-player>"
    QOR_STORE_DEMO_GAME="${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/store/fixtures-demo/v1/packages/org.example.demo.game.ember-climb-1.0.0.qorpkg")
target_link_libraries(tst_storegame PRIVATE Qt6::Test Qt6::Gui)
add_dependencies(tst_storegame qq-player)
add_test(NAME tst_storegame COMMAND tst_storegame)

# Changed for QOR OS: the kernel's wall around a game, from the hostile side (QOR-0018). The Linux wall needs Linux.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_add_executable(tst_playerwall tst_playerwall.cpp ../player/sandbox.cpp)
    target_include_directories(tst_playerwall PRIVATE ../player)
    target_link_libraries(tst_playerwall PRIVATE Qt6::Test Qt6::Gui Qt6::Qml qqruntime)
    add_test(NAME tst_playerwall COMMAND tst_playerwall)
endif()

qt_add_executable(tst_agent tst_agent.cpp)

# The real programs, started as an MCP client and the launcher start them.
target_compile_definitions(tst_agent PRIVATE
    QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_STUDIO="$<TARGET_FILE:qq-studio>"
    QQ_MCP="$<TARGET_FILE:qq-mcp>")

target_link_libraries(tst_agent PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Network
    qqagent qqstudio qqstudioplugin qqruntime qqruntimeplugin
)
add_dependencies(tst_agent qq-studio qq-mcp)

add_test(NAME tst_agent COMMAND tst_agent)

qt_add_executable(tst_lockdown tst_lockdown.cpp)

target_compile_definitions(tst_lockdown PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_link_libraries(tst_lockdown PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::Network
    qqruntime qqruntimeplugin
)

add_test(NAME tst_lockdown COMMAND tst_lockdown)

qt_add_executable(tst_publish tst_publish.cpp)

# The Studio's side of publishing (P3.4), against a stand-in launcher; the launcher's side is in its own tests.
target_compile_definitions(tst_publish PRIVATE QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

target_link_libraries(tst_publish PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Network
    qqstudio qqstudioplugin qqruntime qqruntimeplugin
)

add_test(NAME tst_publish COMMAND tst_publish)

qt_add_executable(tst_workspace tst_workspace.cpp)

# Each project's workspace in the real Studio, from the hostile side too; the agent's run, against a stand-in provider;
# and the real program, for what its command line asks.
target_compile_definitions(tst_workspace PRIVATE
    QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_STUDIO="$<TARGET_FILE:qq-studio>")

target_link_libraries(tst_workspace PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Network
    qqagent qqstudio qqstudioplugin qqruntime qqruntimeplugin
)
add_dependencies(tst_workspace qq-studio)

add_test(NAME tst_workspace COMMAND tst_workspace)

qt_add_executable(tst_gamebuild tst_gamebuild.cpp)

# Changed for QOR OS: making a game of a project, the real Studio and the real Player on what it made.
target_compile_definitions(tst_gamebuild PRIVATE
    QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}/../samples"
    QQ_PLAYER="$<TARGET_FILE:qq-player>"
    QQ_STUDIO="$<TARGET_FILE:qq-studio>")

target_link_libraries(tst_gamebuild PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Network
    qqstudio qqstudioplugin qqruntime qqruntimeplugin
)
add_dependencies(tst_gamebuild qq-player qq-studio)

add_test(NAME tst_gamebuild COMMAND tst_gamebuild)

qt_add_executable(tst_assetlibrary tst_assetlibrary.cpp)

# Changed for QOR OS: the asset library, from the hostile side, the real Studio killed mid-update, the window, and the demonstration
# pack installed by the real StoreEngine where QOR_PRISM_BUILD names a build of Prism.
target_compile_definitions(tst_assetlibrary PRIVATE
    QQ_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    QQ_STUDIO="$<TARGET_FILE:qq-studio>"
    QOR_STORE_DEMO="${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/store/fixtures-demo")

target_link_libraries(tst_assetlibrary PRIVATE
    Qt6::Test Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::QuickControls2 Qt6::Network
    qqstudio qqstudioplugin qqruntime qqruntimeplugin
)
add_dependencies(tst_assetlibrary qq-studio)

add_test(NAME tst_assetlibrary COMMAND tst_assetlibrary)

# Changed for QOR OS: from the build, through Prism's Library where QOR_PRISM_BUILD names a build of it, to the Player.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME first_journey
        COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/dev/first-journey.py"
            --qq-studio $<TARGET_FILE:qq-studio> --qq-player $<TARGET_FILE:qq-player>
            --model "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/orb.gltf")
    set_tests_properties(first_journey PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
endif()

# Changed for QOR OS: these ask the renderer for its name (drawnin.h), which only Qt's private headers can do.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
find_package(Qt6 6.12 REQUIRED COMPONENTS GuiPrivate)
foreach(suite tst_studio tst_play tst_agent)
    target_link_libraries(${suite} PRIVATE Qt6::GuiPrivate)
endforeach()

# Every suite in this folder, so a new one cannot forget.
get_directory_property(qq_suites BUILDSYSTEM_TARGETS)
foreach(suite IN LISTS qq_suites)
    if(suite MATCHES "^tst_[a-z0-9_]+$")
        target_sources(${suite} PRIVATE virtualpads.cpp)
    endif()
endforeach()
