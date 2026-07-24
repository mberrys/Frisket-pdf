#Requires -Version 5.1
<#
.SYNOPSIS
    Validates an installed Frisket-PDF tree on a clean machine (MIC-301).

.DESCRIPTION
    Asserts the shipped layout resolves, runs PdfTool preflight against a fixture,
    optionally exercises the OCR sidecar, launches the Editor, and scans the tree
    for payloads the default bundle is prohibited from shipping.

    This validates an *already installed* tree. To drive the full MSI lifecycle
    (install -> smoke -> upgrade -> uninstall), use Invoke-MsiSmokeTest.ps1, which
    calls this script.

.PARAMETER InstallDir
    Directory containing Pdf4QtEditor.exe and PdfTool.exe.

.PARAMETER ProfilesDir
    Override for the preflight profiles directory. When omitted the script probes
    the layouts CMake can produce (see Resolve-ProfilesDir) and reports which one
    matched -- that resolution is itself a MIC-301 finding worth recording.

.PARAMETER AllowOcrSidecar
    Permit the FrisketOcrService bundle (which carries a Python runtime) to be
    present. docs/PACKAGING_LICENSING.md requires the *default* bundle to be
    C++/Qt only, so this is off by default and the scan fails when it is found.
#>
param(
    [string]$InstallDir = "${env:ProgramFiles}\PDF4QT",
    [string]$ProfilesDir = "",
    [string]$TestPdf = "",
    [switch]$SkipEditorLaunch,
    [switch]$AllowOcrSidecar
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-FileExists {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing $Label`: $Path"
    }
}

function Resolve-ProfilesDir {
    <#
        FRISKET_PREFLIGHT_PROFILES_DIR is ${PDF4QT_INSTALL_SHARE_DIR}/frisket/profiles
        (Pdf4QtEditorPlugins/FrisketPreflightPlugin/CMakeLists.txt). PDF4QT_INSTALL_TO_USR=ON
        -- used by the Windows CI and MSI builds -- prefixes that with usr/, so the share
        tree can sit beside the bin directory or one level above it depending on how the
        installer flattens the staged prefix. Probe rather than assume.
    #>
    param([string]$InstallDir)

    $parent = Split-Path -Parent $InstallDir

    $candidates = @(
        (Join-Path $InstallDir "share\frisket\profiles"),
        (Join-Path $InstallDir "usr\share\frisket\profiles"),
        # Pre-MIC-301 assumption, kept so an old layout still resolves.
        (Join-Path $env:ProgramFiles "share\frisket\profiles")
    )

    # $InstallDir can be a drive root, in which case there is no parent to probe.
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        $candidates += (Join-Path $parent "share\frisket\profiles")
        $candidates += (Join-Path $parent "usr\share\frisket\profiles")
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate "frisket-default.json")) {
            return $candidate
        }
    }

    throw ("Could not locate frisket-default.json. Probed:`n  " + ($candidates -join "`n  ") +
           "`nIf the installer lays profiles down elsewhere, pass -ProfilesDir and update" +
           " docs/PLATFORM_SUPPORT.md to match what actually ships.")
}

function Test-ForbiddenPayload {
    <#
        docs/PACKAGING_LICENSING.md: the default bundle is C++/Qt only -- no Ghostscript,
        no JRE/JDK, no Python. This was a manual checklist item with nothing enforcing it.
    #>
    param([string]$Root, [switch]$AllowOcr)

    $rules = @(
        @{ Label = "Ghostscript"; Patterns = @("gswin*.exe", "gsdll*.dll", "gs.exe") },
        @{ Label = "Java runtime"; Patterns = @("java.exe", "javaw.exe", "jvm.dll", "*.jar") },
        @{ Label = "Python runtime"; Patterns = @("python*.exe", "python3*.dll", "*.whl") }
    )

    $violations = @()
    foreach ($rule in $rules) {
        foreach ($pattern in $rule.Patterns) {
            $hits = @(Get-ChildItem -LiteralPath $Root -Filter $pattern -Recurse -File -ErrorAction SilentlyContinue)
            foreach ($hit in $hits) {
                $isOcrSidecar = $hit.FullName -like "*\FrisketOcrService\*"
                if ($isOcrSidecar -and $AllowOcr) {
                    continue
                }
                $violations += "$($rule.Label): $($hit.FullName)"
            }
        }
    }

    if ($violations.Count -gt 0) {
        $message = "Forbidden payload found in the installed tree (docs/PACKAGING_LICENSING.md):`n  " +
                   ($violations -join "`n  ")
        if (-not $AllowOcr) {
            $message += "`nIf these come from an intentional FrisketOcrService bundle, re-run with -AllowOcrSidecar."
        }
        throw $message
    }

    Write-Host "OK: no Ghostscript / JRE / Python payload in the default bundle"
}

$pluginsDir = Join-Path $InstallDir "pdfplugins"

if ([string]::IsNullOrWhiteSpace($ProfilesDir)) {
    $ProfilesDir = Resolve-ProfilesDir -InstallDir $InstallDir
}
Write-Host "Smoke-testing install at $InstallDir"
Write-Host "Resolved preflight profiles to $ProfilesDir"

$requiredFiles = @(
    @{ Path = (Join-Path $InstallDir "Pdf4QtEditor.exe"); Label = "Editor" },
    @{ Path = (Join-Path $InstallDir "PdfTool.exe"); Label = "PdfTool" },
    @{ Path = (Join-Path $pluginsDir "FrisketPreflightPlugin.dll"); Label = "Frisket preflight plugin" },
    @{ Path = (Join-Path $ProfilesDir "frisket-default.json"); Label = "Default preflight profile" },
    @{ Path = (Join-Path $ProfilesDir "schemas\profile.schema.json"); Label = "Profile schema" },
    @{ Path = (Join-Path $ProfilesDir "schemas\report.schema.json"); Label = "Report schema" }
)

foreach ($item in $requiredFiles) {
    Assert-FileExists -Path $item.Path -Label $item.Label
    Write-Host "OK: $($item.Label)"
}

# The OCR plugin is optional and explicitly out of the V1 gate, so report it rather
# than failing when a slim distribution omits it.
$ocrPlugin = Join-Path $pluginsDir "OcrPlugin.dll"
if (Test-Path -LiteralPath $ocrPlugin) {
    Write-Host "OK: Frisket OCR plugin present"
} else {
    Write-Host "INFO: OcrPlugin.dll absent (optional, not a V1 gate)"
}

if ([string]::IsNullOrWhiteSpace($TestPdf)) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $candidate = Join-Path $repoRoot "frisket-preflight\testdata\fixtures\bleed-adequate.pdf"
    if (Test-Path -LiteralPath $candidate) {
        $TestPdf = $candidate
    }
}

if ([string]::IsNullOrWhiteSpace($TestPdf) -or -not (Test-Path -LiteralPath $TestPdf)) {
    throw "Test PDF not found. Pass -TestPdf pointing at a sample document."
}

$profilePath = Join-Path $ProfilesDir "frisket-default.json"
$pdfTool = Join-Path $InstallDir "PdfTool.exe"
$preflightOutput = & $pdfTool preflight $TestPdf --profile $profilePath --console-format text 2>&1
$preflightExit = $LASTEXITCODE
if ($preflightExit -ne 0 -and $preflightExit -ne 1) {
    throw "PdfTool preflight failed with unexpected exit code $preflightExit`: $preflightOutput"
}
Write-Host "OK: PdfTool preflight completed (exit $preflightExit)"

$ocrSidecar = Join-Path $InstallDir "FrisketOcrService\FrisketOcrService.exe"
if (Test-Path -LiteralPath $ocrSidecar) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $mockSidecar = Join-Path $repoRoot "frisket-ocr\tools\mock_ocr_sidecar.cmd"
    $scanFixture = Join-Path $repoRoot "frisket-preflight\testdata\fixtures\image-dpi-low.pdf"
    if ((Test-Path -LiteralPath $mockSidecar) -and (Test-Path -LiteralPath $scanFixture)) {
        $ocrOutput = & $pdfTool ocr $scanFixture --console-format json --sidecar $mockSidecar 2>&1
        $ocrExit = $LASTEXITCODE
        if ($ocrExit -ne 0 -and $ocrExit -ne 1) {
            throw "PdfTool ocr failed with unexpected exit code $ocrExit`: $ocrOutput"
        }
        Write-Host "OK: PdfTool ocr completed with mock sidecar (exit $ocrExit)"
    }
    Write-Host "OK: FrisketOcrService bundle present"
}

if (-not $SkipEditorLaunch) {
    $editor = Join-Path $InstallDir "Pdf4QtEditor.exe"
    $editorProcess = Start-Process -FilePath $editor -ArgumentList @($TestPdf) -PassThru
    Start-Sleep -Seconds 5
    if ($editorProcess.HasExited) {
        throw "Pdf4QtEditor exited early with code $($editorProcess.ExitCode)"
    }
    Stop-Process -Id $editorProcess.Id -Force
    Write-Host "OK: Pdf4QtEditor launched without immediate crash"
}

Test-ForbiddenPayload -Root $InstallDir -AllowOcr:$AllowOcrSidecar

Write-Host "Smoke test passed."
