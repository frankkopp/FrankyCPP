# Fathom Library Integration
# ===========================
# Fathom is a C library for probing Syzygy endgame tablebases.
# Repository: https://github.com/jdart1/Fathom
# License: MIT
#
# This module downloads and builds Fathom automatically using FetchContent.
# No manual source files needed - everything is fetched during CMake configuration.

include(FetchContent)

message(STATUS "Configuring Fathom tablebase library...")

FetchContent_Declare(
  fathom
  GIT_REPOSITORY https://github.com/jdart1/Fathom.git
  GIT_TAG        master  # TODO: Pin to specific commit for reproducibility
  GIT_SHALLOW    TRUE    # Don't fetch full history
)

# Download the source (but don't call add_subdirectory - Fathom has no CMakeLists.txt)
FetchContent_GetProperties(fathom)
if(NOT fathom_POPULATED)
  FetchContent_Populate(fathom)
endif()

message(STATUS "Fathom source directory: ${fathom_SOURCE_DIR}")

# Create a static library target for Fathom
# Fathom is a pure C library, we need to build it ourselves
add_library(fathom STATIC
  ${fathom_SOURCE_DIR}/src/tbprobe.c
)

# Public headers
target_include_directories(fathom PUBLIC
  ${fathom_SOURCE_DIR}/src
)

# Suppress warnings in third-party code and enable required features
if(MSVC)
  target_compile_options(fathom PRIVATE
    /W0
    /experimental:c11atomics  # Enable C11 atomic support required by Fathom
  )
else()
  target_compile_options(fathom PRIVATE -w)
endif()

# Mark as a system include to suppress warnings in consuming targets
set_target_properties(fathom PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# For MSVC, ensure C11 compatibility
if(MSVC)
  set_target_properties(fathom PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
  )
endif()

message(STATUS "Fathom tablebase library configured successfully")
