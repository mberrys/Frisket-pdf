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

add_executable(UnitTests
	tst_lexicalanalyzertest.cpp
)

target_link_libraries(UnitTests PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTests PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTests  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTests")

add_executable(UnitTestsImageOptimizer
	tst_imageoptimizertest.cpp
)

target_link_libraries(UnitTestsImageOptimizer PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsImageOptimizer PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsImageOptimizer  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsImageOptimizer")

add_executable(UnitTestsBleedFixup
	tst_bleedfixuptest.cpp
)

target_link_libraries(UnitTestsBleedFixup PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

add_executable(UnitTestsDocumentSession
	tst_documentsessiontest.cpp
)

target_link_libraries(UnitTestsDocumentSession PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsDocumentSession PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)


add_executable(UnitTestsFontEncoding
	tst_fontencodingtest.cpp
)

target_link_libraries(UnitTestsFontEncoding PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsFontEncoding PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsFontEncoding  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsFontEncoding")

add_test(UnitTestsDocumentSession "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsDocumentSession")

add_executable(UnitTestsPreflightEngine
	tst_preflightenginetest.cpp
)

target_link_libraries(UnitTestsPreflightEngine PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)
target_compile_definitions(UnitTestsPreflightEngine PRIVATE
    FRISKET_PREFLIGHT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/frisket-preflight")

set_target_properties(UnitTestsPreflightEngine PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsPreflightEngine "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPreflightEngine")

add_executable(UnitTestsBleedMarginProbe
	tst_bleedmarginprobetest.cpp
)

target_link_libraries(UnitTestsBleedMarginProbe PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsBleedMarginProbe PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsBleedMarginProbe "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsBleedMarginProbe")

set_target_properties(UnitTestsBleedFixup PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsBleedFixup  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsBleedFixup")

add_executable(UnitTestsOverprint
	tst_overprinttest.cpp
)

target_link_libraries(UnitTestsOverprint PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Test)

set_target_properties(UnitTestsOverprint PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsOverprint "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsOverprint")

add_executable(UnitTestsPageMasterExport
	tst_pagemasterexporttest.cpp
)

target_link_libraries(UnitTestsPageMasterExport PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test Qt6::Concurrent)

target_compile_definitions(UnitTestsPageMasterExport PRIVATE
    FRISKET_PREFLIGHT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/frisket-preflight"
)

set_target_properties(UnitTestsPageMasterExport PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsPageMasterExport "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPageMasterExport")

add_executable(UnitTestsFilenameSanitizer
	tst_filenamesanitizertest.cpp
)

target_link_libraries(UnitTestsFilenameSanitizer PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Test)

set_target_properties(UnitTestsFilenameSanitizer PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsFilenameSanitizer "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsFilenameSanitizer")

add_executable(UnitTestsRedactVerifier
    tst_redactverifiertest.cpp
)

target_link_libraries(UnitTestsRedactVerifier PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)

set_target_properties(UnitTestsRedactVerifier PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsRedactVerifier "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsRedactVerifier")

# Pure geometry math for the preflight custom checks (bleed / trim / page-size).
# Header-only, so it only needs the PdfTool include dir - no Core or PdfTool link.
add_executable(UnitTestsPreflightChecks
	tst_preflightchecks.cpp
)

target_link_libraries(UnitTestsPreflightChecks PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(UnitTestsPreflightChecks PRIVATE
    ${CMAKE_SOURCE_DIR}/PdfTool
    ${CMAKE_SOURCE_DIR}/Pdf4QtLibCore/sources)

set_target_properties(UnitTestsPreflightChecks PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsPreflightChecks  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPreflightChecks")

# Locks the Editor plugin's pure sidecar path, exit-code, and report-contract helpers.
add_executable(UnitTestsPreflightPlugin
    tst_preflightplugintest.cpp
)

target_link_libraries(UnitTestsPreflightPlugin PRIVATE Qt6::Core Qt6::Test)
target_include_directories(UnitTestsPreflightPlugin PRIVATE ${CMAKE_SOURCE_DIR}/Pdf4QtEditorPlugins/FrisketPreflightPlugin)

set_target_properties(UnitTestsPreflightPlugin PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsPreflightPlugin  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPreflightPlugin")

# Drives `PdfTool preflight` against the frisket-preflight golden corpus, so it needs
# the PdfTool executable, which is not built in PDF4QT_BUILD_ONLY_CORE_LIBRARY mode.
if(NOT PDF4QT_BUILD_ONLY_CORE_LIBRARY)
    add_executable(UnitTestsPreflightCorpus
        tst_preflightcorpus.cpp
    )

    target_link_libraries(UnitTestsPreflightCorpus PRIVATE Qt6::Core Qt6::Test)
    add_dependencies(UnitTestsPreflightCorpus PdfTool)
    target_compile_definitions(UnitTestsPreflightCorpus PRIVATE
        PDFTOOL_EXECUTABLE_PATH="$<TARGET_FILE:PdfTool>"
        FRISKET_PREFLIGHT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/frisket-preflight"
    )

    set_target_properties(UnitTestsPreflightCorpus PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE OFF
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
    )
    add_test(UnitTestsPreflightCorpus  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsPreflightCorpus")

    add_executable(UnitTestsBleedStress
        tst_bleedstresstest.cpp
    )

    target_link_libraries(UnitTestsBleedStress PRIVATE Qt6::Core Qt6::Test)
    add_dependencies(UnitTestsBleedStress PdfTool)
    target_compile_definitions(UnitTestsBleedStress PRIVATE
        PDFTOOL_EXECUTABLE_PATH="$<TARGET_FILE:PdfTool>"
        FRISKET_PREFLIGHT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/frisket-preflight"
    )

    set_target_properties(UnitTestsBleedStress PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE OFF
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
    )
    add_test(UnitTestsBleedStress  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsBleedStress")

    add_executable(UnitTestsOperatorAcceptance
        tst_operatoracceptance.cpp
    )

    target_link_libraries(UnitTestsOperatorAcceptance PRIVATE Qt6::Core Qt6::Test)
    add_dependencies(UnitTestsOperatorAcceptance PdfTool)
    target_compile_definitions(UnitTestsOperatorAcceptance PRIVATE
        PDFTOOL_EXECUTABLE_PATH="$<TARGET_FILE:PdfTool>"
        FRISKET_PREFLIGHT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/frisket-preflight"
    )
    target_include_directories(UnitTestsOperatorAcceptance PRIVATE ${CMAKE_SOURCE_DIR}/Pdf4QtEditorPlugins/FrisketPreflightPlugin)

    set_target_properties(UnitTestsOperatorAcceptance PROPERTIES
        WIN32_EXECUTABLE OFF
        MACOSX_BUNDLE OFF
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
    )
    add_test(UnitTestsOperatorAcceptance  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsOperatorAcceptance")
endif()
