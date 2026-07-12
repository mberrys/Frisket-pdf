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

set_target_properties(UnitTestsBleedFixup PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_LIB_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}
)
add_test(UnitTestsBleedFixup  "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsBleedFixup")

# Pure geometry math for the preflight custom checks (bleed / trim / page-size).
# Header-only, so it only needs the PdfTool include dir - no Core or PdfTool link.
add_executable(UnitTestsPreflightChecks
	tst_preflightchecks.cpp
)

target_link_libraries(UnitTestsPreflightChecks PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
target_include_directories(UnitTestsPreflightChecks PRIVATE ${CMAKE_SOURCE_DIR}/PdfTool)

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
endif()
