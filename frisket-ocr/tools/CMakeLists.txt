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

option(PDF4QT_BUILD_OCR_SERVICE "Build FrisketOcrService via PyInstaller (requires Python venv)" OFF)
option(PDF4QT_BUNDLE_OCR_SERVICE "Install prebuilt FrisketOcrService from frisket-ocr/prebuilt" OFF)

set(FRISKET_OCR_PREBUILT_DIR ${CMAKE_SOURCE_DIR}/frisket-ocr/prebuilt/FrisketOcrService)

if(PDF4QT_BUILD_OCR_SERVICE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(FrisketOcrServiceBundle
        COMMAND ${Python3_EXECUTABLE} -m PyInstaller
            ${CMAKE_SOURCE_DIR}/frisket-ocr/tools/pyinstaller.spec
            --distpath ${CMAKE_SOURCE_DIR}/frisket-ocr/prebuilt
            --workpath ${CMAKE_BINARY_DIR}/frisket-ocr/pyinstaller
            --noconfirm
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/frisket-ocr/tools
        COMMENT "Building FrisketOcrService PyInstaller bundle"
    )
endif()

if(PDF4QT_BUNDLE_OCR_SERVICE)
    if(NOT EXISTS "${FRISKET_OCR_PREBUILT_DIR}/FrisketOcrService.exe" AND NOT EXISTS "${FRISKET_OCR_PREBUILT_DIR}/FrisketOcrService")
        message(WARNING "PDF4QT_BUNDLE_OCR_SERVICE is ON but prebuilt FrisketOcrService was not found at ${FRISKET_OCR_PREBUILT_DIR}")
    else()
        install(DIRECTORY ${FRISKET_OCR_PREBUILT_DIR}/
                DESTINATION ${PDF4QT_INSTALL_BIN_DIR}/FrisketOcrService
                USE_SOURCE_PERMISSIONS)
        install(FILES ${CMAKE_SOURCE_DIR}/frisket-ocr/THIRD_PARTY_NOTICES.txt
                DESTINATION ${PDF4QT_INSTALL_BIN_DIR}/FrisketOcrService)
    endif()
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/frisket-ocr/schemas/
        DESTINATION ${PDF4QT_INSTALL_SHARE_DIR}/frisket/ocr/schemas)
