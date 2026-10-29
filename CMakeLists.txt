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

# --- the .clap ---------------------------------------------------------------------------------
# Hidden visibility, so clap_entry (CLAP_EXPORT in plugin.cpp) is the only exported symbol and two
# plugins with different uw-core pins can't bind to each other's copies.
function(edospread12_add_plugin_sources TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE src/plugin.cpp)
    target_include_directories(${TARGET_NAME} PRIVATE external/clap/include)
    target_link_libraries(${TARGET_NAME} PRIVATE edospread12_core uw::clap-notes)
    target_compile_definitions(${TARGET_NAME} PRIVATE EDOSPREAD12_VERSION_STRING="${PROJECT_VERSION}")
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set_source_files_properties(src/plugin.cpp TARGET_DIRECTORY ${TARGET_NAME}
            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
    endif()
endfunction()

add_library(EDOSpread12 MODULE)
edospread12_add_plugin_sources(EDOSpread12)

if(APPLE)
    set_target_properties(EDOSpread12 PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unlikelyware.edospread12"
        MACOSX_BUNDLE_BUNDLE_NAME "EDOSpread12"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(EDOSPREAD12_CLAP_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
    set(EDOSPREAD12_CLAP_ARTIFACT "$<TARGET_BUNDLE_DIR:EDOSpread12>")
else()
    set_target_properties(EDOSpread12 PROPERTIES PREFIX "" SUFFIX ".clap")
    set(EDOSPREAD12_CLAP_DIR "$ENV{HOME}/.clap")
    set(EDOSPREAD12_CLAP_ARTIFACT "$<TARGET_FILE:EDOSpread12>")
endif()

# A separate target, as in the synths, so an ordinary build never writes outside the build tree.
add_custom_target(install-clap
    COMMAND ${CMAKE_COMMAND} -E make_directory "${EDOSPREAD12_CLAP_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${EDOSPREAD12_CLAP_DIR}/EDOSpread12.clap"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${EDOSPREAD12_CLAP_ARTIFACT}" "${EDOSPREAD12_CLAP_DIR}/EDOSpread12.clap"
    DEPENDS EDOSpread12
    VERBATIM
    COMMENT "Symlinking EDOSpread12.clap into ${EDOSPREAD12_CLAP_DIR}")

# --- tests -------------------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(edospread12-core-tests test/SpreadCoreTests.cpp)
    target_link_libraries(edospread12-core-tests PRIVATE edospread12_core uw::clap-notes)
    add_test(NAME edospread12-core COMMAND edospread12-core-tests)

    # Drives the built .clap over the CLAP C API: no DAW, no GUI.
    add_executable(edospread12-clap-tests test/ClapTests.cpp)
    target_include_directories(edospread12-clap-tests PRIVATE external/clap/include)
    target_link_libraries(edospread12-clap-tests PRIVATE uw::ump ${CMAKE_DL_LIBS})
    add_dependencies(edospread12-clap-tests EDOSpread12)
    add_test(NAME edospread12-clap COMMAND edospread12-clap-tests "${EDOSPREAD12_CLAP_ARTIFACT}")
endif()
