# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
# The generator can rebuild a stale rendering. Every normal build and shell
# link still refuses stale or damaged assets; there is no bypass flag.
add_custom_target(qor_boot_branding_check ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/branding/check_assets.py"
    VERBATIM)
add_custom_target(qor_boot_payload
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/branding/stage_image.py"
        --output "${CMAKE_SOURCE_DIR}/build/boot-branding-payload"
    DEPENDS qor_boot_branding_check
    VERBATIM)
