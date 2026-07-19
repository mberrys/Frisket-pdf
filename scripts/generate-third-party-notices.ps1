#Requires -Version 5.1
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\THIRD_PARTY_NOTICES.txt"),
    [string]$VcpkgJson = (Join-Path $PSScriptRoot "..\vcpkg.json")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$manifest = Get-Content -LiteralPath $VcpkgJson -Raw | ConvertFrom-Json
$lines = @(
    "Frisket-PDF Third-Party Notices",
    "Generated: $(Get-Date -Format o)",
    "",
    "This file lists third-party libraries declared in vcpkg.json.",
    "Refer to each project's license for full terms.",
    ""
)

foreach ($dependency in $manifest.dependencies) {
    $lines += "- $dependency"
}

$lines += ""
$lines += "Qt 6 is also required at runtime when PDF4QT_INSTALL_QT_DEPENDENCIES is enabled."

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
Set-Content -LiteralPath $resolvedOutput -Value ($lines -join [Environment]::NewLine) -Encoding UTF8
Write-Host "Wrote $resolvedOutput"
