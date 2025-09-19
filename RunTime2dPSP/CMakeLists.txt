# PSP build for RunTime2d (no raylib/ImGui). Requires pspsdk toolchain.

cmake_minimum_required(VERSION 3.16)

# Only proceed if building for PSP
if (NOT CMAKE_SYSTEM_NAME STREQUAL "PSP")
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

# Option to build a PRX module instead of a regular ELF
option(PSP_BUILD_PRX "Link as PRX module and package PRX as DATA.PSP" ON)

add_executable(RunTime2dPSP
  src/main.c
)

# Link basic PSP libraries by name
target_link_libraries(RunTime2dPSP PRIVATE
  pspdebug pspdisplay pspge pspctrl pspgu pspgum m
)

# Apply firmware define and (optionally) PRX link flags
target_compile_definitions(RunTime2dPSP PRIVATE PSP_FW_VERSION=${PSP_FW_VERSION})
if (PSP_BUILD_PRX)
  # Add exports for PRX
  target_sources(RunTime2dPSP PRIVATE exports.exp)
  # Link as PRX: use prxspecs and linkfile.prx; avoid startup files
  target_link_options(RunTime2dPSP PRIVATE
    -specs=${PSPDEV}/psp/sdk/lib/prxspecs
    -Wl,-q,-T${PSPDEV}/psp/sdk/lib/linkfile.prx
    -nostartfiles
    -Wl,-zmax-page-size=128
  )
  target_compile_definitions(RunTime2dPSP PRIVATE PSP_BUILD_PRX=1)
endif()

# Output an EBOOT.PBP via post-build if tools are available
# Tools: mksfo/mksfoex to create PARAM.SFO, psp-prxgen to create PRX, pack-pbp to assemble PBP
find_program(PACK_PBP pack-pbp)
find_program(MKSFO NAMES mksfo mksfoex)
find_program(PSP_PRXGEN psp-prxgen)
find_program(PSP_FIXUP_IMPORTS psp-fixup-imports)
find_program(PSP_STRIP psp-strip)

if (PACK_PBP AND MKSFO)
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
    COMMAND ${MKSFO} "RunTime2dPSP" PARAM.SFO
    # Copy icon if present
    COMMAND ${CMAKE_COMMAND} -E echo "ICON0: ${ICON_ARG}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/icon0.png ICON0.PNG
  # Ensure PIC0.PNG exists: copy provided or generate default 1x1 PNG
  COMMAND /bin/sh -lc "if [ -f '${CMAKE_CURRENT_SOURCE_DIR}/pic0.png' ]; then cp '${CMAKE_CURRENT_SOURCE_DIR}/pic0.png' PIC0.PNG; else printf '%s' 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMBApWZC1kAAAAASUVORK5CYII=' | base64 -d > PIC0.PNG; fi"
  COMMAND ${CMAKE_COMMAND} -E echo "PIC0: ${PIC0_ARG}"
  # Copy PIC1 if present
  COMMAND ${CMAKE_COMMAND} -E echo "PIC1: ${PIC1_ARG}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pic1.png PIC1.PNG
    # Select DATA file: PRX when PSP_BUILD_PRX=ON, else ELF
    COMMAND ${CMAKE_COMMAND} -E echo "Preparing DATA for EBOOT (PSP_BUILD_PRX=${PSP_BUILD_PRX})"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:RunTime2dPSP> $<IF:$<BOOL:${PSP_BUILD_PRX}>,BOOT.PRX,BOOT.ELF>
    # Optional strip
    COMMAND ${CMAKE_COMMAND} -E echo "Stripping (if available)"
    COMMAND ${CMAKE_COMMAND} -E env PSPDEV=$ENV{PSPDEV} ${PSP_STRIP} -s $<IF:$<BOOL:${PSP_BUILD_PRX}>,BOOT.PRX,BOOT.ELF> || ${CMAKE_COMMAND} -E echo "psp-strip not found"
    COMMAND ${CMAKE_COMMAND} -E echo "Packing EBOOT.PBP"
  # Order: PARAM.SFO ICON0.PNG ICON1.PMF PIC0.PNG PIC1.PNG SND0.AT3 DATA.PSP [DATA.PSAR]
  COMMAND ${PACK_PBP} EBOOT.PBP PARAM.SFO ${ICON_ARG} NULL ${PIC0_ARG} ${PIC1_ARG} NULL $<IF:$<BOOL:${PSP_BUILD_PRX}>,BOOT.PRX,BOOT.ELF> NULL
    WORKING_DIRECTORY ${PSP_OUTPUT_DIR}
    VERBATIM
  )
endif()
