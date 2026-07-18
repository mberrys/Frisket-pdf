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

set(FRISKET_PREFLIGHT_PLUGIN_SOURCES
    frisketpreflightplugin.cpp
    preflightreportdockwidget.cpp
    preflightreportmodel.cpp
    icons.qrc
)

add_library(FrisketPreflightPlugin SHARED ${FRISKET_PREFLIGHT_PLUGIN_SOURCES})

target_link_libraries(FrisketPreflightPlugin PRIVATE Pdf4QtLibCore Pdf4QtLibWidgets Qt6::Core Qt6::Gui Qt6::Widgets)

set(FRISKET_PREFLIGHT_PROFILES_DIR ${PDF4QT_INSTALL_SHARE_DIR}/frisket/profiles)
cmake_path(RELATIVE_PATH FRISKET_PREFLIGHT_PROFILES_DIR
           BASE_DIRECTORY "${PDF4QT_INSTALL_BIN_DIR}"
           OUTPUT_VARIABLE FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH)

target_compile_definitions(FrisketPreflightPlugin PRIVATE
    FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH="${FRISKET_PREFLIGHT_PROFILES_RELATIVE_PATH}")

set_target_properties(FrisketPreflightPlugin PROPERTIES
                      VERSION ${PDF4QT_VERSION}
                      SOVERSION ${PDF4QT_VERSION}
                      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_PLUGINS_DIR}
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PDF4QT_PLUGINS_DIR})

install(TARGETS FrisketPreflightPlugin RUNTIME DESTINATION ${PDF4QT_PLUGINS_DIR} LIBRARY DESTINATION ${PDF4QT_PLUGINS_DIR})
install(FILES ${CMAKE_SOURCE_DIR}/frisket-preflight/profiles/frisket-default.json
        DESTINATION ${FRISKET_PREFLIGHT_PROFILES_DIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/frisket-preflight/schemas/
        DESTINATION ${FRISKET_PREFLIGHT_PROFILES_DIR}/schemas)
