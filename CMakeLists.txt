# MIT License
#
# Copyright (c) 2018-2025 Jakub Melka and Contributors
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.16)

set(PDF4QT_VERSION 1.6.0.0)
message(STATUS "Build type - CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

project(PDF4QT VERSION ${PDF4QT_VERSION} LANGUAGES CXX)

message("VCPKG Enabled: " ${VCPKG_TOOLCHAIN})

option(PDF4QT_INSTALL_DEPENDENCIES "Install dependencies" ${VCPKG_TOOLCHAIN})
option(PDF4QT_INSTALL_QT_DEPENDENCIES "Install Qt dependencies" ${VCPKG_TOOLCHAIN})
option(PDF4QT_INSTALL_INCLUDE "Install include headers" ON)
option(PDF4QT_FLATPAK_BUILD "Special flag for Flatpak" OFF)

add_compile_definitions(PDF4QT_PROJECT_VERSION="${PDF4QT_VERSION}")
add_compile_definitions(QT_NO_EMIT)

if(PDF4QT_FLATPAK_BUILD)
    add_compile_definitions(PDF4QT_FLATPAK_BUILD)
endif()

if(WIN32 AND MSVC)
    option(PDF4QT_INSTALL_MSVC_REDISTRIBUTABLE "Install MSVC redistributable package" ON)
    option(PDF4QT_INSTALL_PREPARE_WIX_INSTALLER "Prepare Wix installer for Windows" ON)
endif()

option(PDF4QT_BUILD_ONLY_CORE_LIBRARY "Build only core library" OFF)
option(PDF4QT_BUILD_TESTS "Build UnitTests" ON)
option(PDF4QT_ENABLE_SANITIZERS "Build with ASan+UBSan" OFF)
option(PDF4QT_BUILD_FUZZERS "Build libFuzzer harnesses" OFF)
if(WIN32)
    set(_PDF4QT_ENABLE_SENTRY_DEFAULT ON)
else()
    set(_PDF4QT_ENABLE_SENTRY_DEFAULT OFF)
endif()
option(PDF4QT_ENABLE_SENTRY "Enable Sentry Native crash reporting (Windows-first)" ${_PDF4QT_ENABLE_SENTRY_DEFAULT})
set(PDF4QT_SENTRY_DSN "" CACHE STRING "Optional compile-time Sentry DSN (prefer SENTRY_DSN env at runtime)")

if(PDF4QT_ENABLE_SENTRY)
    find_package(Threads REQUIRED)
    find_package(sentry CONFIG REQUIRED)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Pdf4QtSentry.cmake)
endif()

# Frisket slim release profile: disable apps/plugins without deleting upstream sources.
option(PDF4QT_FRISKET_DISTRIBUTION "Slim Frisket release (disable non-product apps and plugins)" OFF)

if(PDF4QT_FRISKET_DISTRIBUTION)
    set(_PDF4QT_BUILD_CODE_GENERATOR_DEFAULT OFF)
    set(_PDF4QT_BUILD_JBIG2_VIEWER_DEFAULT OFF)
    set(_PDF4QT_BUILD_EXAMPLE_GENERATOR_DEFAULT OFF)
    set(_PDF4QT_BUILD_VIEWER_DEFAULT OFF)
    set(_PDF4QT_BUILD_PAGEMASTER_DEFAULT OFF)
    set(_PDF4QT_BUILD_DIFF_DEFAULT OFF)
    set(_PDF4QT_BUILD_LAUNCHPAD_DEFAULT OFF)
    set(_PDF4QT_PLUGIN_AUDIOBOOK_DEFAULT OFF)
    set(_PDF4QT_PLUGIN_SCANNER_DEFAULT OFF)
else()
    set(_PDF4QT_BUILD_CODE_GENERATOR_DEFAULT ON)
    set(_PDF4QT_BUILD_JBIG2_VIEWER_DEFAULT ON)
    set(_PDF4QT_BUILD_EXAMPLE_GENERATOR_DEFAULT ON)
    set(_PDF4QT_BUILD_VIEWER_DEFAULT ON)
    set(_PDF4QT_BUILD_PAGEMASTER_DEFAULT ON)
    set(_PDF4QT_BUILD_DIFF_DEFAULT ON)
    set(_PDF4QT_BUILD_LAUNCHPAD_DEFAULT ON)
    set(_PDF4QT_PLUGIN_AUDIOBOOK_DEFAULT ON)
    set(_PDF4QT_PLUGIN_SCANNER_DEFAULT ON)
endif()

option(PDF4QT_BUILD_CODE_GENERATOR "Build CodeGenerator developer tool" ${_PDF4QT_BUILD_CODE_GENERATOR_DEFAULT})
option(PDF4QT_BUILD_JBIG2_VIEWER "Build JBIG2_Viewer developer tool" ${_PDF4QT_BUILD_JBIG2_VIEWER_DEFAULT})
option(PDF4QT_BUILD_EXAMPLE_GENERATOR "Build PdfExampleGenerator developer tool" ${_PDF4QT_BUILD_EXAMPLE_GENERATOR_DEFAULT})
option(PDF4QT_BUILD_VIEWER "Build Pdf4QtViewer application" ${_PDF4QT_BUILD_VIEWER_DEFAULT})
option(PDF4QT_BUILD_PAGEMASTER "Build Pdf4QtPageMaster application" ${_PDF4QT_BUILD_PAGEMASTER_DEFAULT})
option(PDF4QT_BUILD_DIFF "Build Pdf4QtDiff application" ${_PDF4QT_BUILD_DIFF_DEFAULT})
option(PDF4QT_BUILD_LAUNCHPAD "Build Pdf4QtLaunchPad application" ${_PDF4QT_BUILD_LAUNCHPAD_DEFAULT})
option(PDF4QT_PLUGIN_AUDIOBOOK "Build AudioBook editor plugin" ${_PDF4QT_PLUGIN_AUDIOBOOK_DEFAULT})
option(PDF4QT_PLUGIN_SCANNER "Build Scanner editor plugin" ${_PDF4QT_PLUGIN_SCANNER_DEFAULT})

if(PDF4QT_ENABLE_SANITIZERS OR PDF4QT_BUILD_FUZZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(PDF4QT_QT_ROOT "" CACHE PATH "Qt root directory")

if(VCPKG_TOOLCHAIN AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Installation directory" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core LinguistTools)

if(PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg Xml)
else()
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg Xml PrintSupport TextToSpeech Concurrent)
endif()

if(PDF4QT_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES en de cs es ko zh_CN zh_TW fr tr ru)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenJPEG CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(blend2d CONFIG REQUIRED)

if(VCPKG_TOOLCHAIN)
    find_package(lcms2 REQUIRED)
    set(LCMS2_LIBRARIES lcms2::lcms2 CACHE INTERNAL "LCMS2 libraries")
else() # lcms usually doesn't provide a config file
    find_library(LCMS2_LIBRARIES lcms2 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_AUTORCC_OPTIONS "--threshold;0;--compress;9")

if (MSVC)
    add_compile_options(/bigobj /W4 /wd5054 /wd4127 /wd4702)
endif()

if (MINGW)
    add_compile_options("-Wa,-mbig-obj")
endif()

if (UNIX AND NOT APPLE AND CMAKE_COMPILER_IS_GNUCXX)
    set(LINUX_GCC ON)
endif()

if (UNIX AND NOT APPLE)
    set(PDF4QT_LINUX ON)
endif()

# Parallel STL (libstdc++/libc++) on Linux needs TBB for both GCC and Clang.
if(PDF4QT_LINUX)
    find_package(TBB CONFIG REQUIRED)
    set(PDF4QT_USE_TBB ON)
endif()

option(PDF4QT_INSTALL_TO_USR "Install to usr directory" ${VCPKG_TOOLCHAIN})

if(PDF4QT_LINUX)
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_LIBDIR})
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
else()
    set(PDF4QT_INSTALL_LIB_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
    set(PDF4QT_INSTALL_BIN_DIR_IMPL ${CMAKE_INSTALL_BINDIR})
endif()

if(PDF4QT_INSTALL_TO_USR)
    set(PDF4QT_INSTALL_LIB_DIR usr/${PDF4QT_INSTALL_LIB_DIR_IMPL})
    set(PDF4QT_INSTALL_BIN_DIR usr/${PDF4QT_INSTALL_BIN_DIR_IMPL})
    set(PDF4QT_INSTALL_SHARE_DIR usr/${CMAKE_INSTALL_DATAROOTDIR})
    set(PDF4QT_INSTALL_INCLUDE_DIR usr/${CMAKE_INSTALL_INCLUDEDIR})
else()
    set(PDF4QT_INSTALL_LIB_DIR ${PDF4QT_INSTALL_LIB_DIR_IMPL})
    set(PDF4QT_INSTALL_BIN_DIR ${PDF4QT_INSTALL_BIN_DIR_IMPL})
    set(PDF4QT_INSTALL_SHARE_DIR ${CMAKE_INSTALL_DATAROOTDIR})
    set(PDF4QT_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR})
endif()

if(PDF4QT_LINUX)
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/pdf4qt)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_SHARE_DIR}/pdf4qt/translations)
else()
    set(PDF4QT_PLUGINS_DIR ${PDF4QT_INSTALL_LIB_DIR}/pdfplugins)
    set(PDF4QT_INSTALL_TRANS_DIR ${PDF4QT_INSTALL_BIN_DIR}/translations)
endif()

# Relative paths are used by the config
cmake_path(RELATIVE_PATH PDF4QT_PLUGINS_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE PDF4QT_PLUGINS_RELATIVE_PATH)
cmake_path(RELATIVE_PATH PDF4QT_INSTALL_TRANS_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE PDF4QT_TRANSLATIONS_RELATIVE_PATH)
configure_file(config.h.cmake config.h)

add_subdirectory(Pdf4QtLibCore)

if(PDF4QT_INSTALL_INCLUDE)
    # Install public headers from Pdf4QtLibCore/include
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Pdf4QtLibCore/sources/
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibCore
            FILES_MATCHING PATTERN "*.h")

    # Install generated export header
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pdf4qtlibcore_export.h
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibCore)

    # Install public headers from Pdf4QtLibWidgets/include
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Pdf4QtLibWidgets/sources/
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibWidgets
            FILES_MATCHING PATTERN "*.h")

    # Install generated export header
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pdf4qtlibwidgets_export.h
            DESTINATION ${PDF4QT_INSTALL_INCLUDE_DIR}/Pdf4QtLibWidgets)
endif()

if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    add_subdirectory(Pdf4QtLibWidgets)
    if(PDF4QT_BUILD_CODE_GENERATOR)
        add_subdirectory(CodeGenerator)
    endif()
    if(PDF4QT_BUILD_JBIG2_VIEWER)
        add_subdirectory(JBIG2_Viewer)
    endif()
    if(PDF4QT_BUILD_EXAMPLE_GENERATOR)
        add_subdirectory(PdfExampleGenerator)
    endif()
    add_subdirectory(PdfTool)
    add_subdirectory(frisket-preflight/tools)
    add_subdirectory(Pdf4QtLibGui)
    add_subdirectory(Pdf4QtEditorPlugins)
    add_subdirectory(Pdf4QtEditor)
    if(PDF4QT_BUILD_VIEWER)
        add_subdirectory(Pdf4QtViewer)
    endif()
    if(PDF4QT_BUILD_PAGEMASTER)
        add_subdirectory(Pdf4QtPageMaster)
    endif()
    if(PDF4QT_BUILD_DIFF)
        add_subdirectory(Pdf4QtDiff)
    endif()
    if(PDF4QT_BUILD_LAUNCHPAD)
        add_subdirectory(Pdf4QtLaunchPad)
    endif()
    add_subdirectory(WixInstaller)
endif()

if(PDF4QT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(UnitTests)
endif()

if(PDF4QT_BUILD_FUZZERS)
    add_subdirectory(Fuzz)
endif()

qt_collect_translation_source_targets(i18n_targets)
message("CMAKE_TRANSLATION_TARGETS = " ${i18n_targets})

set(PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/translations)
set(PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_TRANS_DIR})

message("PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY" ${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY})
message("PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY" ${PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY})

# Only add translation targets for targets that actually exist in this build.
foreach(_tgt Pdf4QtEditor Pdf4QtViewer Pdf4QtPageMaster Pdf4QtDiff PdfTool)
    if(TARGET ${_tgt})
        list(APPEND _existing_translation_targets ${_tgt})
    endif()
endforeach()
if(_existing_translation_targets)
    qt_add_translations(TARGETS ${_existing_translation_targets}
                        SOURCE_TARGETS ${i18n_targets}
                        MERGE_QT_TRANSLATIONS
                        LUPDATE_OPTIONS -no-obsolete
                        TS_OUTPUT_DIRECTORY ${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY}
                        QM_OUTPUT_DIRECTORY ${PDF4QT_TRANSLATION_QM_OUTPUT_DIRECTORY}
                        QM_FILES_OUTPUT_VARIABLE PDF4QT_QM_FILES)
endif()

if(WIN32)
    set(PDF4QT_LUPDATE_TARGET "${PROJECT_NAME}_lupdate")
    if(TARGET ${PDF4QT_LUPDATE_TARGET})
        add_custom_command(TARGET ${PDF4QT_LUPDATE_TARGET}
                           POST_BUILD
                           COMMAND ${CMAKE_COMMAND}
                                   -DTS_OUTPUT_DIRECTORY=${PDF4QT_TRANSLATION_TS_OUTPUT_DIRECTORY}
                                   -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeTsLineEndings.cmake
                           COMMENT "Normalizing TS files to CRLF")
    endif()
endif()

message("CMAKE_PREFIX_PATH = " ${CMAKE_PREFIX_PATH})
message("CMAKE_TOOLCHAIN_FILE = " ${CMAKE_TOOLCHAIN_FILE})

configure_file(version.txt.in version.txt)

if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    install(FILES ${PDF4QT_QM_FILES}
            DESTINATION ${PDF4QT_INSTALL_TRANS_DIR})

    if(WIN32)
        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Desktop/44x44/
                RUNTIME DESTINATION ${PDF4QT_INSTALL_BIN_DIR}/assets/44x44
                FILES_MATCHING
                PATTERN *.png)

        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Desktop/150x150/
                RUNTIME DESTINATION ${PDF4QT_INSTALL_BIN_DIR}/assets/150x150
                FILES_MATCHING
                PATTERN *.png)

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/AppxManifest.xml.in ${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml)
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AppxManifest.xml DESTINATION ${PDF4QT_INSTALL_BIN_DIR})
        install(FILES assets/logo.png DESTINATION ${PDF4QT_INSTALL_BIN_DIR}/assets)
    else()
        configure_file(make-package.sh.in make-package.sh)

        install(FILES Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtDiff.desktop
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtPageMaster.desktop
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtViewer.desktop
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtEditor.desktop
                      Desktop/io.github.mberrys.Frisket-pdf.desktop
                      DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/applications)
        install(FILES Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtDiff.svg
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtPageMaster.svg
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtViewer.svg
                      Desktop/io.github.mberrys.Frisket-pdf.Pdf4QtEditor.svg
                      Desktop/io.github.mberrys.Frisket-pdf.svg
                      DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/icons/hicolor/scalable/apps)
        install(FILES Desktop/io.github.mberrys.Frisket-pdf.appdata.xml DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/metainfo)
        install(FILES Desktop/128x128/io.github.mberrys.Frisket-pdf.Pdf4QtDiff.png
                      Desktop/128x128/io.github.mberrys.Frisket-pdf.Pdf4QtPageMaster.png
                      Desktop/128x128/io.github.mberrys.Frisket-pdf.Pdf4QtViewer.png
                      Desktop/128x128/io.github.mberrys.Frisket-pdf.Pdf4QtEditor.png
                      Desktop/128x128/io.github.mberrys.Frisket-pdf.png
                      DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/icons/hicolor/128x128/apps)
    endif()

endif()

if(PDF4QT_INSTALL_DEPENDENCIES AND NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    install(DIRECTORY ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/
        RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/
        FILES_MATCHING
        PATTERN "*.dll"
        PATTERN "*.so"
        PATTERN "*.dylib"
    )

    if("${PDF4QT_QT_ROOT}" STREQUAL "")
        message(WARNING "Set variable PDF4QT_QT_ROOT to Qt6 root directory")
    endif()

    if(PDF4QT_INSTALL_QT_DEPENDENCIES)
        install(DIRECTORY ${PDF4QT_QT_ROOT}/bin/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/
            FILES_MATCHING
            REGEX "(Qt6Core|Qt6Gui|Qt6PrintSupport|Qt6Svg|Qt6TextToSpeech|Qt6Widgets|Qt6Xml|Qt6Multimedia|Qt6Network)\\..*"
            PATTERN "Debug" EXCLUDE
            PATTERN "*.pdb" EXCLUDE
        )

        install(DIRECTORY ${PDF4QT_QT_ROOT}/plugins/platforms/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/platforms/
            FILES_MATCHING
            PATTERN "qwindows.dll"
            PATTERN "qoffscreen.dll"
            PATTERN "*.so"
            PATTERN "*.dylib"
            PATTERN "*.pdb" EXCLUDE
        )

        install(DIRECTORY ${PDF4QT_QT_ROOT}/plugins/iconengines/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/iconengines/
            FILES_MATCHING
            REGEX "qsvgicon\\..*"
            PATTERN "*.pdb" EXCLUDE
        )

        install(DIRECTORY ${PDF4QT_QT_ROOT}/plugins/imageformats/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/imageformats/
            FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "*.so"
            PATTERN "*.dylib"
            REGEX "d\\..*" EXCLUDE
            PATTERN "*.pdb" EXCLUDE
        )

        install(DIRECTORY ${PDF4QT_QT_ROOT}/plugins/styles/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/styles/
            FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "*.so"
            PATTERN "*.dylib"
            REGEX "d\\..*" EXCLUDE
            PATTERN "*.pdb" EXCLUDE
        )

        install(DIRECTORY ${PDF4QT_QT_ROOT}/plugins/texttospeech/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/texttospeech/
            FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "*.so"
            PATTERN "*.dylib"
            REGEX "d\\..*" EXCLUDE
            PATTERN "*.pdb" EXCLUDE
        )
    endif()

    if(WIN32 AND MSVC AND PDF4QT_INSTALL_MSVC_REDISTRIBUTABLE)
        if(DEFINED ENV{MSVC_REDISTRIBUTABLES_PATH})
            set(MSVC_REDISTRIBUTABLES_PATH $ENV{MSVC_REDISTRIBUTABLES_PATH})
            message(STATUS "Using existing MSVC Redistributable Package Path = ${MSVC_REDISTRIBUTABLES_PATH}")
        else()
            get_filename_component(MSVC_REDISTRIBUTABLES_PATH $ENV{VCToolsRedistDir}/$ENV{VSCMD_ARG_TGT_ARCH}/Microsoft.VC${MSVC_TOOLSET_VERSION}.CRT/ ABSOLUTE)
            message(STATUS "MSVC Redistributable Package Path = ${MSVC_REDISTRIBUTABLES_PATH}")
        endif()

        install(DIRECTORY ${MSVC_REDISTRIBUTABLES_PATH}/
            RUNTIME DESTINATION ${PDF4QT_INSTALL_LIB_DIR}/
            FILES_MATCHING
            PATTERN "*.dll"
        )
    endif()
endif()

if(PDF4QT_ENABLE_SENTRY AND TARGET sentry_crashpad::crashpad_handler)
    get_target_property(_pdf4qt_sentry_crashpad_handler_install sentry_crashpad::crashpad_handler IMPORTED_LOCATION_RELEASE)
    if(NOT _pdf4qt_sentry_crashpad_handler_install)
        get_target_property(_pdf4qt_sentry_crashpad_handler_install sentry_crashpad::crashpad_handler IMPORTED_LOCATION)
    endif()

    if(_pdf4qt_sentry_crashpad_handler_install)
        if(WIN32)
            install(PROGRAMS ${_pdf4qt_sentry_crashpad_handler_install}
                    DESTINATION ${PDF4QT_INSTALL_BIN_DIR}
                    RENAME crashpad_handler.exe)
        else()
            install(PROGRAMS ${_pdf4qt_sentry_crashpad_handler_install}
                    DESTINATION ${PDF4QT_INSTALL_BIN_DIR}
                    RENAME crashpad_handler)
        endif()
    endif()

    if(TARGET sentry_crashpad::crashpad_wer)
        get_target_property(_pdf4qt_sentry_crashpad_wer_install sentry_crashpad::crashpad_wer IMPORTED_LOCATION_RELEASE)
        if(NOT _pdf4qt_sentry_crashpad_wer_install)
            get_target_property(_pdf4qt_sentry_crashpad_wer_install sentry_crashpad::crashpad_wer IMPORTED_LOCATION)
        endif()

        if(_pdf4qt_sentry_crashpad_wer_install)
            install(FILES ${_pdf4qt_sentry_crashpad_wer_install}
                    DESTINATION ${PDF4QT_INSTALL_BIN_DIR})
        endif()
    endif()
endif()

#macro(dump_variables)
#    message(STATUS "dump_variables------------------------------------------{")
#    get_cmake_property(_variableNames VARIABLES)
#    foreach (_variableName ${_variableNames})
#        message(STATUS "${_variableName}=${${_variableName}}")
#    endforeach()
#    message(STATUS "dump_variables------------------------------------------}")
#endmacro()
#
#dump_variables()

