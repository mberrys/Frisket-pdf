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

if(WIN32 AND MSVC AND PDF4QT_INSTALL_PREPARE_WIX_INSTALLER)
    if(PDF4QT_INSTALL_TO_USR)
        file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/usr/bin" WIX_INSTALL_PREFIX)
    else()
        file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}/bin" WIX_INSTALL_PREFIX)
    endif()
    file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/WixInstaller" PDF4QT_WIX_DIR)

    if(PDF4QT_FRISKET_DISTRIBUTION)
        set(PDF4QT_WIX_PLUGIN_PACK_DESCRIPTION "Plugin pack for editor: Frisket preflight, signatures, soft-proofing, dimensions, object inspector, and redaction.")
    elseif(PDF4QT_PLUGIN_AUDIOBOOK)
        set(PDF4QT_WIX_PLUGIN_PACK_DESCRIPTION "Plugin pack for editor. Contains additional editing features, such as audio book conversion, signatures, soft-proofing, dimensions, object inspector, redaction etc.")
    else()
        set(PDF4QT_WIX_PLUGIN_PACK_DESCRIPTION "Plugin pack for editor. Contains signatures, soft-proofing, dimensions, object inspector, redaction, and Frisket preflight.")
    endif()

    if(PDF4QT_BUILD_VIEWER)
        set(PDF4QT_WIX_FEATURE_VIEWER [=[
      <Feature Id="feature_PDF4QT_Viewer" Absent="allow" AllowAdvertise="no" Title="Viewer" Level="1" Description="Simple PDF viewer with basic functions.">
        <ComponentGroupRef Id="PDF4QT_Viewer"/>
      </Feature>
]=])
        set(PDF4QT_WIX_FRAGMENT_VIEWER [=[
  <Fragment>
    <ComponentGroup Id="PDF4QT_Viewer">
      <Component Id="cmpPdf4QtViewer" Directory="INSTALLFOLDER" Guid="{2D222775-6DA7-43E6-B212-3A142622190C}">
        <File Id="filPdf4QtViewer" KeyPath="yes" Source="$(var.MyInstallDir)\Pdf4QtViewer.exe">
          <Shortcut Id="PDF4QT_Viewer_Shortcut" Directory="ProgramMenuDir" Name="PDF4QT Viewer" Description="Simple PDF viewer" WorkingDirectory="INSTALLFOLDER" Icon="PDF4QT_VL_Icon.exe" IconIndex="0" Advertise="yes"/>
        </File>
        <ProgId Id='pdf4qt_default_viewer.pdf' Description='PDF Document'>
          <Extension Id='pdf' ContentType='application/pdf'>
            <Verb Id='open' Command='Open' TargetFile='filPdf4QtViewer' Argument='"%1"' />
          </Extension>
        </ProgId>
      </Component>
    </ComponentGroup>
  </Fragment>

  <Fragment>
    <Icon Id="PDF4QT_VL_Icon.exe" SourceFile="$(var.MyInstallDir)\Pdf4QtViewer.exe"/>
  </Fragment>
]=])
    else()
        set(PDF4QT_WIX_FEATURE_VIEWER "")
        set(PDF4QT_WIX_FRAGMENT_VIEWER "")
    endif()

    if(PDF4QT_BUILD_PAGEMASTER)
        set(PDF4QT_WIX_FEATURE_PAGEMASTER [=[
      <Feature Id="feature_PDF4QT_PageMaster" Absent="allow" AllowAdvertise="no" Title="PageMaster" Level="1" Description="Document page organizer (split/merge documents, insert/remove/move/clone pages, insert blank pages and images to create a new document).">
        <ComponentGroupRef Id="PDF4QT_PageMaster"/>
      </Feature>
]=])
        set(PDF4QT_WIX_FRAGMENT_PAGEMASTER [=[
  <Fragment>
    <ComponentGroup Id="PDF4QT_PageMaster">
      <Component Id="cmpPdf4QtPageMaster" Directory="INSTALLFOLDER" Guid="{9D248BEE-BA78-4050-B362-0694B11AA326}">
        <File Id="filPdf4QtPageMaster" KeyPath="yes" Source="$(var.MyInstallDir)\Pdf4QtPageMaster.exe">
          <Shortcut Id="PDF4QT_DPO_Shortcut" Directory="ProgramMenuDir" Name="PDF4QT PageMaster" Description="PDF Document page organizer" WorkingDirectory="INSTALLFOLDER" Icon="PDF4QT_DPO_Icon.exe" IconIndex="0" Advertise="yes"/>
        </File>
      </Component>
    </ComponentGroup>
  </Fragment>

  <Fragment>
    <Icon Id="PDF4QT_DPO_Icon.exe" SourceFile="$(var.MyInstallDir)\Pdf4QtPageMaster.exe"/>
  </Fragment>
]=])
    else()
        set(PDF4QT_WIX_FEATURE_PAGEMASTER "")
        set(PDF4QT_WIX_FRAGMENT_PAGEMASTER "")
    endif()

    if(PDF4QT_BUILD_DIFF)
        set(PDF4QT_WIX_FEATURE_DIFF [=[
      <Feature Id="feature_PDF4QT_Diff" Absent="allow" AllowAdvertise="no" Title="Diff" Level="1" Description="Compare content of two documents.">
        <ComponentGroupRef Id="PDF4QT_Diff"/>
      </Feature>
]=])
        set(PDF4QT_WIX_FRAGMENT_DIFF [=[
  <Fragment>
    <ComponentGroup Id="PDF4QT_Diff">
      <Component Id="cmpPdf4QtDiff" Directory="INSTALLFOLDER" Guid="{067D40E9-4FAE-472D-BCC3-B92A23CA0028}">
        <File Id="filPdf4QtDiff" KeyPath="yes" Source="$(var.MyInstallDir)\Pdf4QtDiff.exe">
          <Shortcut Id="PDF4QT_DD_Shortcut" Directory="ProgramMenuDir" Name="PDF4QT Diff" Description="Compare two similar PDF documents" WorkingDirectory="INSTALLFOLDER" Icon="PDF4QT_DD_Icon.exe" IconIndex="0" Advertise="yes"/>
        </File>
      </Component>
    </ComponentGroup>
  </Fragment>

  <Fragment>
    <Icon Id="PDF4QT_DD_Icon.exe" SourceFile="$(var.MyInstallDir)\Pdf4QtDiff.exe"/>
  </Fragment>
]=])
    else()
        set(PDF4QT_WIX_FEATURE_DIFF "")
        set(PDF4QT_WIX_FRAGMENT_DIFF "")
    endif()

    if(PDF4QT_PLUGIN_AUDIOBOOK)
        set(PDF4QT_WIX_PLUGIN_AUDIOBOOK [=[
      <Component Id="cmpAudioBookPlugin" Directory="dir_pdfplugins" Guid="{06D159D2-0F34-417E-9134-7E9195E697A0}">
        <File Id="filAudioBookPlugin" KeyPath="yes" Source="$(var.MyInstallDir)\pdfplugins\AudioBookPlugin.dll" />
      </Component>
]=])
    else()
        set(PDF4QT_WIX_PLUGIN_AUDIOBOOK "")
    endif()

    if(PDF4QT_PLUGIN_SCANNER)
        set(PDF4QT_WIX_PLUGIN_SCANNER [=[
      <Component Id="cmpScannerPlugin" Directory="dir_pdfplugins" Guid="{B7E4A1C2-3D5F-4E89-9A1B-2C8D7E6F5A40}">
        <File Id="filScannerPlugin" KeyPath="yes" Source="$(var.MyInstallDir)\pdfplugins\ScannerPlugin.dll" />
      </Component>
]=])
    else()
        set(PDF4QT_WIX_PLUGIN_SCANNER "")
    endif()

    set(PDF4QT_WIX_PLUGIN_OCR [=[
      <Component Id="cmpOcrPlugin" Directory="dir_pdfplugins" Guid="{C8F5B2D3-4E6A-4F91-8B2C-3D9E8F7A6B51}">
        <File Id="filOcrPlugin" KeyPath="yes" Source="$(var.MyInstallDir)\pdfplugins\OcrPlugin.dll" />
      </Component>
]=])

    if(PDF4QT_BUNDLE_OCR_SERVICE)
        set(PDF4QT_WIX_FEATURE_OCR_SERVICE [=[
          <ComponentGroupRef Id="PDF4QT_Ocr_Service"/>
]=])
        set(PDF4QT_WIX_FRAGMENT_OCR_SERVICE [=[
  <Fragment>
    <DirectoryRef Id="INSTALLFOLDER">
      <Directory Id="dir_FrisketOcrService" Name="FrisketOcrService" />
    </DirectoryRef>
  </Fragment>

  <Fragment>
    <ComponentGroup Id="PDF4QT_Ocr_Service">
      <Component Id="cmpFrisketOcrServiceExe" Directory="dir_FrisketOcrService" Guid="{D9A6C3E4-5F7B-4A02-9C3D-4E0F9A8B7C62}">
        <File Id="filFrisketOcrServiceExe" KeyPath="yes" Source="$(var.MyInstallDir)\FrisketOcrService\FrisketOcrService.exe" />
      </Component>
    </ComponentGroup>
  </Fragment>
]=])
    else()
        set(PDF4QT_WIX_FEATURE_OCR_SERVICE "")
        set(PDF4QT_WIX_FRAGMENT_OCR_SERVICE "")
    endif()

    if(PDF4QT_ENABLE_SENTRY)
        set(PDF4QT_WIX_SENTRY_CRASHPAD [=[
      <Component Id="cmpCrashpadHandler" Directory="INSTALLFOLDER" Guid="{F4A2C8E1-6B3D-4F90-9C1A-2E7D5B8A4F31}">
        <File Id="filCrashpadHandler" KeyPath="yes" Source="$(var.MyInstallDir)\crashpad_handler.exe" />
      </Component>
]=])
    else()
        set(PDF4QT_WIX_SENTRY_CRASHPAD "")
    endif()

    configure_file(PDF4QT.sln.in PDF4QT.sln)
    configure_file(PDF4QT.wixproj.in PDF4QT.wixproj)
    configure_file(Product.wxs.in Product.wxs)
    configure_file(createInstaller.ps1.in createInstaller.ps1)
    configure_file(explic.rtf explic.rtf)
endif()
