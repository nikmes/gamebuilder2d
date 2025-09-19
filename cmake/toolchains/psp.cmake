# Minimal PSP CMake toolchain leveraging pspsdk (pspdev).
# Expects PSPDEV env var or defaults to /usr/local/pspdev.

set(PSPDEV $ENV{PSPDEV})
if(NOT PSPDEV)
  set(PSPDEV /usr/local/pspdev)
endif()

set(CMAKE_SYSTEM_NAME PSP)
set(CMAKE_SYSTEM_PROCESSOR mipsel)

# Compilers
set(CMAKE_C_COMPILER   ${PSPDEV}/bin/psp-gcc)
set(CMAKE_CXX_COMPILER ${PSPDEV}/bin/psp-g++)
set(CMAKE_ASM_COMPILER ${PSPDEV}/bin/psp-gcc)

# Sysroot
set(CMAKE_SYSROOT ${PSPDEV}/psp)

# Flags
set(CMAKE_C_FLAGS_INIT "-G0")
set(CMAKE_CXX_FLAGS_INIT "-G0")

# Linker
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-q")

# Paths
set(CMAKE_FIND_ROOT_PATH ${PSPDEV}/psp)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# PSPSDK-provided CMake modules (if any)
list(APPEND CMAKE_MODULE_PATH "${PSPDEV}/psp/cmake")
