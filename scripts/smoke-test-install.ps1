#Requires -Version 5.1
param(
    [string]$InstallDir = "${env:ProgramFiles}\PDF4QT",
    [string]$TestPdf = "",
    [switch]$SkipEditorLaunch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Assert-FileExists {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing $Label`: $Path"
    }
}

$shareProfilesDir = Join-Path $env:ProgramFiles "share\frisket\profiles"
$pluginsDir = Join-Path $InstallDir "pdfplugins"

Write-Host "Smoke-testing install at $InstallDir"

$requiredFiles = @(
    @{ Path = (Join-Path $InstallDir "Pdf4QtEditor.exe"); Label = "Editor" },
    @{ Path = (Join-Path $InstallDir "PdfTool.exe"); Label = "PdfTool" },
    @{ Path = (Join-Path $pluginsDir "FrisketPreflightPlugin.dll"); Label = "Frisket preflight plugin" },
    @{ Path = (Join-Path $shareProfilesDir "frisket-default.json"); Label = "Default preflight profile" },
    @{ Path = (Join-Path $shareProfilesDir "schemas\profile.schema.json"); Label = "Profile schema" },
    @{ Path = (Join-Path $shareProfilesDir "schemas\report.schema.json"); Label = "Report schema" }
)

foreach ($item in $requiredFiles) {
    Assert-FileExists -Path $item.Path -Label $item.Label
    Write-Host "OK: $($item.Label)"
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

$profilePath = Join-Path $shareProfilesDir "frisket-default.json"
$pdfTool = Join-Path $InstallDir "PdfTool.exe"
$preflightOutput = & $pdfTool preflight $TestPdf --profile $profilePath --console-format text 2>&1
$preflightExit = $LASTEXITCODE
if ($preflightExit -ne 0 -and $preflightExit -ne 1) {
    throw "PdfTool preflight failed with unexpected exit code $preflightExit`: $preflightOutput"
}
Write-Host "OK: PdfTool preflight completed (exit $preflightExit)"

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

Write-Host "Smoke test passed."
