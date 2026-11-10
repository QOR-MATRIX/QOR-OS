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

# Match the Pi 5's 2712 kernel. Every dependency, including Qt, needs its own
# ELF layout check; these flags affect only the libraries and executables we link.
# common-page-size also keeps RELRO effective on the 16-KiB kernel (GNU ld).
foreach(qor_pi_kind EXE SHARED MODULE)
    string(APPEND CMAKE_${qor_pi_kind}_LINKER_FLAGS_INIT
        " -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384")
endforeach()
