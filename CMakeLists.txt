cmake_minimum_required(VERSION 3.21)   # clap-wrapper needs 3.21 (M5)

# macOS: 26 or later, Apple Silicon only. Set before project();
# ignored on other platforms. Both are cache variables, so -D overrides them.
set(CMAKE_OSX_DEPLOYMENT_TARGET 26.0 CACHE STRING "macOS 26 or later")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only")
project(EDOSpread12 VERSION 0.1.0 LANGUAGES CXX)

# The core is linked into the .clap, a shared module (M2), so it must be position-independent.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- uw-core (git submodule): uw::ump and uw::clap-notes, no CLAP host, JACK or ALSA ---------
# EDOSPREAD12_UW_CORE_DIR points at another uw-core clone (e.g. ../uw-core) to build
# against a change that isn't pushed and pinned yet.
set(EDOSPREAD12_UW_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/uw-core" CACHE PATH "uw-core source")
set(UW_CORE_UMP_ONLY ON CACHE BOOL "" FORCE)
set(UW_CORE_TESTS OFF CACHE BOOL "" FORCE)
# uw::clap-notes (ClapNoteRemap, the CLAP note dialect) builds against EDOSpread12's own CLAP SDK.
set(UW_CORE_CLAP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/clap/include" CACHE PATH "" FORCE)
add_subdirectory(${EDOSPREAD12_UW_CORE_DIR} uw-core)

# --- core: EDOSpread12's rules over uw::ump, no CLAP ------------------------------------------
add_library(edospread12_core STATIC src/SpreadCore.cpp)
target_include_directories(edospread12_core PUBLIC src)
target_link_libraries(edospread12_core PUBLIC uw::ump)
set_target_properties(edospread12_core PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(edospread12_core PRIVATE -Wall -Wextra -Wconversion)
endif()

# --- tests -------------------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(edospread12-core-tests test/SpreadCoreTests.cpp)
    target_link_libraries(edospread12-core-tests PRIVATE edospread12_core uw::clap-notes)
    add_test(NAME edospread12-core COMMAND edospread12-core-tests)
endif()
