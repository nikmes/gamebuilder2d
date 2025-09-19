# PSP build for RunTime2d (no raylib/ImGui). Requires pspsdk toolchain.

cmake_minimum_required(VERSION 3.16)

# Only proceed if building for PSP
if (NOT CMAKE_SYSTEM_NAME STREQUAL "PSP" AND NOT BUILD_FOR_PSP)
  message(STATUS "RunTime2dPSP: Skipping (CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})")
  return()
endif()

project(RunTime2dPSP C)

set(CMAKE_C_STANDARD 99)

# pspsdk provides a CMake toolchain that sets includes/libs
# Expect PSPDEV env var to be set when toolchain is active

# Make sure SDK include/lib paths are visible
set(PSPDEV $ENV{PSPDEV})
if (NOT PSPDEV)
  set(PSPDEV /usr/local/pspdev)
endif()
include_directories(${PSPDEV}/psp/sdk/include)
link_directories(${PSPDEV}/psp/sdk/lib)

# Firmware/API level (default 660). Can override with -DPSP_FW_VERSION=XXX
set(PSP_FW_VERSION "${PSP_FW_VERSION}" CACHE STRING "PSP firmware API version (e.g. 660)")
if (NOT PSP_FW_VERSION)
  set(PSP_FW_VERSION 660)
endif()

# EBOOT metadata (title, ID, app version, category)
set(PSP_EBOOT_TITLE "${PSP_EBOOT_TITLE}" CACHE STRING "EBOOT Title shown in XMB")
if (NOT PSP_EBOOT_TITLE)
  set(PSP_EBOOT_TITLE "RunTime2dPSP")
endif()
set(PSP_EBOOT_ID "${PSP_EBOOT_ID}" CACHE STRING "EBOOT Title ID (e.g. HBRET001)")
if (NOT PSP_EBOOT_ID)
  set(PSP_EBOOT_ID "HBRUN001")
endif()
set(PSP_APP_VER "${PSP_APP_VER}" CACHE STRING "Application version string")
if (NOT PSP_APP_VER)
  set(PSP_APP_VER "01.00")
endif()
set(PSP_CATEGORY "${PSP_CATEGORY}" CACHE STRING "CATEGORY field (e.g. HG for homebrew)")
if (NOT PSP_CATEGORY)
  set(PSP_CATEGORY "HG")
endif()

# Convert firmware numeric to dotted string (e.g. 660 -> 6.60)
math(EXPR _fw_major "${PSP_FW_VERSION} / 100")
math(EXPR _fw_minor "(${PSP_FW_VERSION} % 100) / 10")
math(EXPR _fw_patch "${PSP_FW_VERSION} % 10")
set(PSP_SYSTEM_VER_STR "${_fw_major}.${_fw_minor}${_fw_patch}")

# Option to build a PRX module instead of a regular ELF
option(PSP_BUILD_PRX "Link as PRX module and package PRX as DATA.PSP" OFF)

add_executable(RunTime2dPSP
  src/main.c
)

# Link basic PSP libraries by name
target_link_libraries(RunTime2dPSP PRIVATE
  pspdebug pspdisplay pspge pspctrl pspgu pspgum psprtc psputility m z stdc++
)

# Apply firmware define and (optionally) PRX link flags
target_compile_definitions(RunTime2dPSP PRIVATE PSP_FW_VERSION=${PSP_FW_VERSION})
target_compile_definitions(RunTime2dPSP PRIVATE PSP _PSP)
target_compile_options(RunTime2dPSP PRIVATE -O2 -G0 -ffast-math)
if (PSP_BUILD_PRX)
  # Add exports for PRX but link as normal ELF; we'll prxgen in packaging
  target_sources(RunTime2dPSP PRIVATE exports.exp)
  target_compile_definitions(RunTime2dPSP PRIVATE PSP_BUILD_PRX=1)
  target_link_libraries(RunTime2dPSP PRIVATE pspkernel)
  target_link_options(RunTime2dPSP PRIVATE
    -specs=${PSPDEV}/psp/sdk/lib/prxspecs
    -Wl,-q,-T${PSPDEV}/psp/sdk/lib/linkfile.prx
    -nostartfiles
    -Wl,-zmax-page-size=128
  )
endif()

# Output an EBOOT.PBP via post-build if tools are available
# Tools: mksfo/mksfoex to create PARAM.SFO, psp-prxgen to create PRX, pack-pbp to assemble PBP
find_program(PACK_PBP pack-pbp)
find_program(MKSFO mksfo)
find_program(MKSFOEX mksfoex)
find_program(PSP_PRXGEN psp-prxgen)
find_program(PSP_FIXUP_IMPORTS psp-fixup-imports)
find_program(PSP_STRIP psp-strip)

if (PACK_PBP AND (MKSFO OR MKSFOEX))
  set(PSP_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

  # Determine icon argument and copy only if provided
  set(ICON_ARG NULL)
  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png)
    set(ICON_ARG ICON0.PNG)
  endif()

  # Determine PIC1 (background) argument
  set(PIC1_ARG NULL)
  if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/pic1.png)
    set(PIC1_ARG PIC1.PNG)
  endif()

  # Always provide PIC0.PNG (title image). If source is missing, generate a tiny default PNG.
  set(PIC0_ARG PIC0.PNG)

  add_custom_command(TARGET RunTime2dPSP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Preparing PSP assets"
    # Generate PARAM.SFO initially (will be overridden in PRX branch with simple mksfo)
    COMMAND /bin/sh -lc "set -e; if [ -n '${MKSFOEX}' ]; then echo 'Using mksfoex to create PARAM.SFO'; \"${MKSFOEX}\" -d CATEGORY=${PSP_CATEGORY} -d BOOTABLE=1 -d PSP_SYSTEM_VER=${PSP_FW_VERSION} -d DISC_ID=${PSP_EBOOT_ID} -d TITLE_ID=${PSP_EBOOT_ID} -s APP_VER=${PSP_APP_VER} '${PSP_EBOOT_TITLE}' PARAM.SFO; else echo 'Using mksfo to create PARAM.SFO'; \"${MKSFO}\" '${PSP_EBOOT_TITLE}' PARAM.SFO; fi"
    # Copy icon if present
    COMMAND ${CMAKE_COMMAND} -E echo "ICON0: ${ICON_ARG}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png ICON0.PNG
  # Ensure PIC0.PNG exists: copy provided or generate default 1x1 PNG
  COMMAND /bin/sh -lc "if [ -f '${CMAKE_CURRENT_SOURCE_DIR}/pic0.png' ]; then cp '${CMAKE_CURRENT_SOURCE_DIR}/pic0.png' PIC0.PNG; else printf '%s' 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMBApWZC1kAAAAASUVORK5CYII=' | base64 -d > PIC0.PNG; fi"
  COMMAND ${CMAKE_COMMAND} -E echo "PIC0: ${PIC0_ARG}"
  # Copy PIC1 if present
  COMMAND ${CMAKE_COMMAND} -E echo "PIC1: ${PIC1_ARG}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pic1.png PIC1.PNG
    # Select PRX or ELF path using a shell branch; then pack EBOOT.PBP
  COMMAND /bin/sh -lc "set -e; echo 'Preparing DATA for EBOOT (PSP_BUILD_PRX=${PSP_BUILD_PRX})'; if [ \"${PSP_BUILD_PRX}\" = \"ON\" ]; then cp $<TARGET_FILE:RunTime2dPSP> EBOOT.BIN; if command -v ${PSP_FIXUP_IMPORTS} >/dev/null 2>&1; then ${PSP_FIXUP_IMPORTS} EBOOT.BIN || true; fi; if command -v ${MKSFO} >/dev/null 2>&1; then \"${MKSFO}\" '${PSP_EBOOT_TITLE}' PARAM.SFO; fi; if command -v ${PSP_PRXGEN} >/dev/null 2>&1; then ${PSP_PRXGEN} EBOOT.BIN BOOT.PRX; else cp EBOOT.BIN BOOT.PRX; fi; if command -v ${PSP_STRIP} >/dev/null 2>&1; then ${PSP_STRIP} -s BOOT.PRX || true; fi; chmod +w EBOOT.PBP 2>/dev/null || true; rm -f EBOOT.PBP; echo 'Packing EBOOT (PRX)'; ${PACK_PBP} EBOOT.PBP PARAM.SFO ${ICON_ARG} NULL ${PIC0_ARG} ${PIC1_ARG} NULL BOOT.PRX NULL; else cp $<TARGET_FILE:RunTime2dPSP> EBOOT.BIN; if command -v ${PSP_FIXUP_IMPORTS} >/dev/null 2>&1; then ${PSP_FIXUP_IMPORTS} EBOOT.BIN || true; fi; if command -v ${PSP_STRIP} >/dev/null 2>&1; then ${PSP_STRIP} -s EBOOT.BIN || true; fi; chmod +w EBOOT.PBP 2>/dev/null || true; rm -f EBOOT.PBP; echo 'Packing EBOOT (ELF)'; ${PACK_PBP} EBOOT.PBP PARAM.SFO ${ICON_ARG} NULL ${PIC0_ARG} ${PIC1_ARG} NULL EBOOT.BIN NULL; fi"
    WORKING_DIRECTORY ${PSP_OUTPUT_DIR}
    VERBATIM
  )
endif()
