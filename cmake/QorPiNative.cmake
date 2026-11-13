# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# QOR-0025's native application build, not a cross-build or an image recipe.
# Included before compiler discovery so a wrong host fails without running Qt tools.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
   OR NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$"
   OR CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Q Pi requires a native ARM64 Linux builder; no emulated or cross-build acceptance")
endif()

file(READ "/proc/sys/kernel/osrelease" qor_pi_kernel LIMIT 4096)
if(qor_pi_kernel MATCHES "[Mm]icrosoft|WSL")
    message(FATAL_ERROR "Q Pi requires native Linux, not WSL")
endif()
if(NOT IS_ABSOLUTE "$ENV{QOR_QT_DIR}" OR NOT IS_DIRECTORY "$ENV{QOR_QT_DIR}/lib/cmake/Qt6")
    message(FATAL_ERROR "QOR_QT_DIR must name the qualified commercial Qt 6.12 Linux ARM64 kit")
endif()
if(NOT QOR_HARDENING OR NOT CMAKE_COMPILE_WARNING_AS_ERROR OR QQ_PLAYER_ONLY)
    message(FATAL_ERROR "Q Pi builds Studio, Player and tests with hardening and warnings as errors")
endif()

# A cached component path wins over a prefix even with Qt's NO_DEFAULT_PATH.
# Refuse mixed-kit hints without rewriting them. Include installed package names
# so environment-only hints are checked too; no kit CMake file is included here.
file(REAL_PATH "$ENV{QOR_QT_DIR}" qor_pi_qt_root)
get_cmake_property(qor_pi_qt_hints VARIABLES)
list(FILTER qor_pi_qt_hints INCLUDE REGEX "^Qt6[A-Za-z0-9]*_(DIR|ROOT)$")
file(GLOB qor_pi_qt_packages LIST_DIRECTORIES TRUE "${qor_pi_qt_root}/lib/cmake/Qt6*")
foreach(qor_pi_qt_package IN LISTS qor_pi_qt_packages)
    get_filename_component(qor_pi_qt_name "${qor_pi_qt_package}" NAME)
    list(APPEND qor_pi_qt_hints "${qor_pi_qt_name}_DIR" "${qor_pi_qt_name}_ROOT")
endforeach()
list(APPEND qor_pi_qt_hints Qt6_DIR Qt6_ROOT)
list(REMOVE_DUPLICATES qor_pi_qt_hints)
function(qor_pi_require_qt_hint value expected)
    if("${value}" STREQUAL "" OR "${value}" MATCHES "-NOTFOUND$")
        return()
    endif()
    if(NOT IS_ABSOLUTE "${value}" OR "${value}" MATCHES ";")
        message(FATAL_ERROR "Q Pi refuses conflicting Qt package paths; use the checked QOR_QT_DIR kit")
    endif()
    file(REAL_PATH "${value}" qor_pi_qt_hint_real)
    file(REAL_PATH "${expected}" qor_pi_qt_expected_real)
    if(NOT qor_pi_qt_hint_real STREQUAL qor_pi_qt_expected_real)
        message(FATAL_ERROR "Q Pi refuses conflicting Qt package paths; use the checked QOR_QT_DIR kit")
    endif()
endfunction()
foreach(qor_pi_qt_hint IN LISTS qor_pi_qt_hints)
    if(qor_pi_qt_hint MATCHES "_DIR$")
        string(REGEX REPLACE "_DIR$" "" qor_pi_qt_name "${qor_pi_qt_hint}")
        set(qor_pi_qt_expected "${qor_pi_qt_root}/lib/cmake/${qor_pi_qt_name}")
    else()
        set(qor_pi_qt_expected "${qor_pi_qt_root}")
    endif()
    qor_pi_require_qt_hint("${${qor_pi_qt_hint}}" "${qor_pi_qt_expected}")
    qor_pi_require_qt_hint("$CACHE{${qor_pi_qt_hint}}" "${qor_pi_qt_expected}")
    qor_pi_require_qt_hint("$ENV{${qor_pi_qt_hint}}" "${qor_pi_qt_expected}")
endforeach()
if(QT_DISABLE_NO_DEFAULT_PATH_IN_QT_PACKAGES)
    message(FATAL_ERROR "Q Pi requires Qt's restricted package lookup")
endif()
foreach(qor_pi_qt_hint QT_ADDITIONAL_PACKAGES_PREFIX_PATH QT_ADDITIONAL_HOST_PACKAGES_PREFIX_PATH
                       QT_BUILD_CMAKE_PREFIX_PATH QT_HOST_PATH)
    qor_pi_require_qt_hint("${${qor_pi_qt_hint}}" "${qor_pi_qt_root}")
    qor_pi_require_qt_hint("$CACHE{${qor_pi_qt_hint}}" "${qor_pi_qt_root}")
    qor_pi_require_qt_hint("$ENV{${qor_pi_qt_hint}}" "${qor_pi_qt_root}")
endforeach()
set(Qt6_DIR "${qor_pi_qt_root}/lib/cmake/Qt6")

# Use the host's Python, not package discovery through the unqualified Qt prefix.
# This runs before find_package(Qt6) and compiler discovery in the native preset.
# Explicitly supplied CMake toolchains have already run before top-level includes;
# this gate is not a sandbox for an owner's arbitrary CMake configuration.
unset(qor_pi_python)
unset(qor_pi_python CACHE)
find_program(qor_pi_python NAMES python3 python NAMES_PER_DIR PATHS ENV PATH NO_DEFAULT_PATH NO_CACHE)
if(NOT qor_pi_python)
    message(FATAL_ERROR "Q Pi needs an existing host Python interpreter for its read-only Qt gate")
endif()
execute_process(
    COMMAND "${qor_pi_python}" -E -s -B "${CMAKE_CURRENT_LIST_DIR}/../tools/image/pi_preflight.py" --qt-only
            --qt-target "$ENV{QOR_QT_DIR}"
    RESULT_VARIABLE qor_pi_qt_result
    OUTPUT_QUIET ERROR_QUIET
    TIMEOUT 15)
if(NOT qor_pi_qt_result STREQUAL "0")
    message(FATAL_ERROR "Q Pi Qt metadata/Core layout check failed; inspect pi_preflight.py --qt-only --qt-target <kit>")
endif()
# A passing early check does not certify the remaining modules, dependency ABI,
# entitlement, resource budget or native application operation.

# Match the Pi 5's 2712 kernel. Every dependency, including Qt, needs its own
# ELF layout check; these flags affect only the libraries and executables we link.
# common-page-size also keeps RELRO effective on the 16-KiB kernel (GNU ld).
foreach(qor_pi_kind EXE SHARED MODULE)
    string(APPEND CMAKE_${qor_pi_kind}_LINKER_FLAGS_INIT
        " -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
endforeach()
