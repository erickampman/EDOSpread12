cmake_minimum_required(VERSION 3.21)

# macOS: 26 or later, Apple Silicon only. Set before project();
# ignored on other platforms. Both are cache variables, so -D overrides them.
set(CMAKE_OSX_DEPLOYMENT_TARGET 26.0 CACHE STRING "macOS 26 or later")
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Apple Silicon only")
project(EDOSpread12 VERSION 0.9.0 LANGUAGES C CXX)   # C for DPF's pugl and NanoVG sources
if(APPLE)
    enable_language(OBJCXX)   # DPF's DistrhoUI_macOS.mm and our BackingScale_mac.mm
endif()

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
add_library(edospread12_core STATIC src/SpreadCore.cpp src/EdoFacts.cpp)
target_include_directories(edospread12_core PUBLIC src)
target_link_libraries(edospread12_core PUBLIC uw::ump)
set_target_properties(edospread12_core PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(edospread12_core PRIVATE -Wall -Wextra -Wconversion)
endif()

# --- DPF, only for the editor (src/UI) --------------------------------------------------------
# UIExporter + NanoVG + pugl, as NoteGran uses it: the CLAP entry point stays ours. dgl-opengl is
# what dpf_add_plugin() would make; SHARED_RESOURCES embeds DGL's DejaVu Sans.
add_subdirectory(external/dpf)
dpf__add_dgl_opengl(TRUE FALSE FALSE)
target_compile_options(dgl-opengl PRIVATE -w)   # vendored: its warnings aren't ours
# EDOSpread12's own namespace, as every DPF user's: on macOS pugl names its Objective-C classes
# after it, so two DPF plugins with the default ("DGL") in one host would collide.
target_compile_definitions(dgl-opengl PUBLIC DGL_NAMESPACE=EDOSpread12DGL)

# resources/* compiled in as bytes with DPF's res2c.py: the logo, read with NanoVG at run time, and
# the help page, which the editor's Help button writes out and opens. Not help.md: res2c.py names
# each symbol by the file's stem, so it would collide with help.html.
#
# help.html is generated from help.md (beside it) by pandoc with the Unlikelyware products' shared
# template, and committed; after editing help.md, run from here:
#   pandoc -f commonmark_x-smart -t html5 \
#     --template ../../Dev-Audio/Shared/Help/help-template.html \
#     --metadata pagetitle=help-EDOSpread12 resources/help.md -o resources/help.html
# (the template, in Dev-Audio beside Dev-MIDI, inlines its help.css).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(EDOSPREAD12_RESOURCES_CPP "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.cpp")
file(GLOB EDOSPREAD12_RESOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/*.png"
    "${CMAKE_SOURCE_DIR}/resources/*.html")
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/generated/EmbeddedResources.hpp" "${EDOSPREAD12_RESOURCES_CPP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/generated/resources"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated/resources"
    COMMAND ${CMAKE_COMMAND} -E copy ${EDOSPREAD12_RESOURCE_FILES} "${CMAKE_BINARY_DIR}/generated/resources"
    COMMAND ${Python3_EXECUTABLE} "${DPF_ROOT_DIR}/utils/res2c.py"
            EmbeddedResources "${CMAKE_BINARY_DIR}/generated/resources" "${CMAKE_BINARY_DIR}/generated"
    DEPENDS ${EDOSPREAD12_RESOURCE_FILES} "${DPF_ROOT_DIR}/utils/res2c.py"
    COMMENT "Embedding resources/* as compiled-in data")

# --- the .clap ---------------------------------------------------------------------------------
# Hidden visibility, so clap_entry (CLAP_EXPORT in plugin.cpp) is the only exported symbol and two
# plugins with different uw-core pins can't bind to each other's copies.
function(edospread12_add_plugin_sources TARGET_NAME)
    target_sources(${TARGET_NAME} PRIVATE src/plugin.cpp src/UI/EDOSpread12UI.cpp "${EDOSPREAD12_RESOURCES_CPP}"
        "${DPF_ROOT_DIR}/distrho/src/DistrhoUI.cpp" "${DPF_ROOT_DIR}/distrho/src/DistrhoUtils.cpp")
    target_include_directories(${TARGET_NAME} PRIVATE external/clap/include src src/UI
        "${CMAKE_BINARY_DIR}/generated" "${DPF_ROOT_DIR}/distrho")
    target_link_libraries(${TARGET_NAME} PRIVATE edospread12_core uw::clap-notes dgl-opengl)
    target_compile_definitions(${TARGET_NAME} PRIVATE EDOSPREAD12_VERSION_STRING="${PROJECT_VERSION}"
        DISTRHO_PLUGIN_TARGET_CLAP DISTRHO_IS_STANDALONE=0)
    dpf__add_plugin_specific_ui_sources(${TARGET_NAME} FALSE)   # macOS: DistrhoUI_macOS.mm
    if(APPLE)
        target_sources(${TARGET_NAME} PRIVATE src/UI/BackingScale_mac.mm)   # the window's live Retina scale
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set_source_files_properties(src/plugin.cpp src/UI/EDOSpread12UI.cpp TARGET_DIRECTORY ${TARGET_NAME}
            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;$<$<CXX_COMPILER_ID:GNU>:-Wno-cpp>")
    endif()
    # DGL's libraries keep default visibility, so the linker is told too: clap_entry is the only
    # export (another plugin's DPF or uw-core in the same host can't bind to ours).
    if(APPLE)
        target_link_options(${TARGET_NAME} PRIVATE "LINKER:-exported_symbol,_clap_entry")
    elseif(UNIX)
        file(WRITE "${CMAKE_BINARY_DIR}/generated/exports.map" "{ global: clap_entry; local: *; };\n")
        target_link_options(${TARGET_NAME} PRIVATE "LINKER:--version-script=${CMAKE_BINARY_DIR}/generated/exports.map")
    endif()
endfunction()

add_library(EDOSpread12 MODULE)
edospread12_add_plugin_sources(EDOSpread12)

if(APPLE)
    set_target_properties(EDOSpread12 PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/clap-Info.plist.in"
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

# --- tools/preview_editor.mm: the editor in a window of its own, no DAW (macOS) ----------------
if(APPLE)
    # A small app bundle, so the window is an ordinary app's (screenshots, accessibility).
    add_executable(preview-editor MACOSX_BUNDLE tools/preview_editor.mm)
    set_target_properties(preview-editor PROPERTIES
        OUTPUT_NAME "EDOSpread12 Preview"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.unlikelyware.edospread12.preview"
        MACOSX_BUNDLE_BUNDLE_NAME "EDOSpread12 Preview")
    target_include_directories(preview-editor PRIVATE external/clap/include)
    target_compile_options(preview-editor PRIVATE -fobjc-arc)
    target_link_libraries(preview-editor PRIVATE "-framework Cocoa")
    add_dependencies(preview-editor EDOSpread12)
endif()

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
