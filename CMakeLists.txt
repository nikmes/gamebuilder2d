# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.16)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("GameBuilder2d")

# Avoid MSVC PDB contention during parallel builds
if (MSVC)
  add_compile_options(/FS)
endif()

# Include sub-projects conditionally by platform/toolchain
set(BUILD_FOR_PSP "${BUILD_FOR_PSP}" CACHE BOOL "Force building only PSP targets")
if (BUILD_FOR_PSP OR CMAKE_SYSTEM_NAME STREQUAL "PSP")
  add_subdirectory ("RunTime2dPSP")
else()
  add_subdirectory ("GameBuilder2d")
  add_subdirectory ("RunTime2d")
endif()
