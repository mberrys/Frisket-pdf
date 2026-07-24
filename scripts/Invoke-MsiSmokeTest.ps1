#Requires -Version 5.1
<#
.SYNOPSIS
    Drives the full Windows MSI lifecycle on a clean machine (MIC-301).

.DESCRIPTION
    install -> smoke -> [upgrade -> smoke] -> uninstall -> assert removal.

    smoke-test-install.ps1 validates an already-installed tree; this wraps it with
    the lifecycle the packaging checklist requires (clean-machine install, upgrade,
    uninstall) and which had never been executed against a real MSI.

    Run from an elevated PowerShell on a VM with no MSVC, no Qt, and no Python.

.PARAMETER MsiPath
    The MSI under test.

.PARAMETER PreviousMsiPath
    Optional older MSI. When supplied it is installed first and $MsiPath is applied
    over it, exercising the upgrade path.

.PARAMETER InstallDir
    Expected install directory. Defaults to the 64-bit Program Files.

    NOTE: WindowsInstall.yml invokes `candle ... -arch x86` while the payload is x64
    (--triplet x64-windows, win64_msvc2022_64). If the MSI lands under
    "Program Files (x86)" this run will fail here -- that is a real finding, not a
    script bug. Record it on MIC-301 and fix the WiX arch before sign-off.

.EXAMPLE
    .\Invoke-MsiSmokeTest.ps1 -MsiPath .\mberrys.Frisket-pdf_1.6.0.0.msi
#>
param(
    [Parameter(Mandatory = $true)][string]$MsiPath,
    [string]$PreviousMsiPath = "",
    [string]$InstallDir = "${env:ProgramFiles}\PDF4QT",
    [string]$TestPdf = "",
    [string]$LogDir = "$env:TEMP\frisket-msi-smoke",
    [switch]$SkipEditorLaunch,
    [switch]$AllowOcrSidecar
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$smokeScript = Join-Path $PSScriptRoot "smoke-test-install.ps1"
if (-not (Test-Path -LiteralPath $smokeScript)) {
    throw "Missing sibling script: $smokeScript"
}
if (-not (Test-Path -LiteralPath $MsiPath)) {
    throw "MSI not found: $MsiPath"
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

function Invoke-Msi {
    param([string]$Arguments, [string]$LogName)

    $logPath = Join-Path $LogDir "$LogName.log"
    $fullArgs = "$Arguments /qn /norestart /l*v `"$logPath`""
    Write-Host "msiexec $fullArgs"
    $process = Start-Process -FilePath "msiexec.exe" -ArgumentList $fullArgs -Wait -PassThru

    # 3010 == success, reboot required. Treat as success for a smoke run.
    if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) {
        throw "msiexec failed with exit code $($process.ExitCode). Verbose log: $logPath"
    }
    Write-Host "OK: msiexec succeeded (exit $($process.ExitCode)); log at $logPath"
}

function Invoke-Smoke {
    param([string]$Stage)

    Write-Host "--- Smoke test ($Stage) ---"
    $smokeArgs = @{ InstallDir = $InstallDir }
    if (-not [string]::IsNullOrWhiteSpace($TestPdf)) { $smokeArgs.TestPdf = $TestPdf }
    if ($SkipEditorLaunch.IsPresent) { $smokeArgs.SkipEditorLaunch = $true }
    if ($AllowOcrSidecar.IsPresent) { $smokeArgs.AllowOcrSidecar = $true }

    # smoke-test-install.ps1 sets $ErrorActionPreference = "Stop" and signals failure by
    # throwing, so the exception propagates through & without an exit-code check.
    & $smokeScript @smokeArgs
    Write-Host "--- Smoke test passed ($Stage) ---"
}

if (Test-Path -LiteralPath $InstallDir) {
    throw ("$InstallDir already exists. This test must run on a clean machine so that " +
           "a stale tree cannot mask a packaging defect. Remove it or snapshot back first.")
}

if (-not [string]::IsNullOrWhiteSpace($PreviousMsiPath)) {
    Write-Host "=== Installing previous version for upgrade coverage ==="
    Invoke-Msi -Arguments "/i `"$PreviousMsiPath`"" -LogName "install-previous"
    Invoke-Smoke -Stage "previous version"

    Write-Host "=== Upgrading to version under test ==="
    Invoke-Msi -Arguments "/i `"$MsiPath`"" -LogName "upgrade"
    Invoke-Smoke -Stage "after upgrade"
} else {
    Write-Host "=== Installing version under test ==="
    Invoke-Msi -Arguments "/i `"$MsiPath`"" -LogName "install"
    Invoke-Smoke -Stage "fresh install"
}

Write-Host "=== Uninstalling ==="
Invoke-Msi -Arguments "/x `"$MsiPath`"" -LogName "uninstall"

if (Test-Path -LiteralPath $InstallDir) {
    $leftovers = @(Get-ChildItem -LiteralPath $InstallDir -Recurse -File -ErrorAction SilentlyContinue)
    if ($leftovers.Count -gt 0) {
        throw ("Uninstall left $($leftovers.Count) file(s) behind in $InstallDir`:`n  " +
               (($leftovers | Select-Object -First 20 | ForEach-Object { $_.FullName }) -join "`n  "))
    }
    Write-Host "INFO: $InstallDir remains as an empty directory after uninstall"
} else {
    Write-Host "OK: install directory fully removed"
}

Write-Host ""
Write-Host "MSI lifecycle smoke test passed. Attach this transcript to MIC-301."
