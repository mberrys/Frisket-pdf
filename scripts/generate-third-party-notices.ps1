#Requires -Version 5.1
<#
.SYNOPSIS
    Generates THIRD_PARTY_NOTICES.txt for a Frisket-PDF release.

.DESCRIPTION
    Resolves each vcpkg dependency to a concrete version and license text, preferring
    the actual installed vcpkg tree (authoritative for what shipped) and falling back
    to the license files committed under 3rdparty_licenses/.

    docs/PACKAGING_LICENSING.md requires notices generated from the *final packaged
    artifact*. This narrows that gap but does not close it: it reads the dependency
    manifest and vcpkg tree, not the built installer. A full SBOM from the artifact is
    still required before paid distribution.

.PARAMETER VcpkgInstalledDir
    vcpkg install root (the directory holding <triplet>/ and vcpkg/status). Defaults to
    $env:VCPKG_INSTALLED_DIR. When absent, the script falls back to committed licenses.

.PARAMETER Strict
    Fail if any dependency resolves without license text. Use for release runs.
#>
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\THIRD_PARTY_NOTICES.txt"),
    [string]$VcpkgJson = (Join-Path $PSScriptRoot "..\vcpkg.json"),
    [string]$VcpkgInstalledDir = $env:VCPKG_INSTALLED_DIR,
    [string]$Triplet = "x64-windows",
    [switch]$Strict
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$committedLicenseDir = Join-Path $repoRoot "3rdparty_licenses"

# Ports whose license text is committed in-repo, used when the vcpkg tree is unavailable.
$fallbackLicenseFiles = @{
    "lcms"          = "LittleCMS_COPYING.txt"
    "openjpeg"      = "OpenJPEG_LICENSE.txt"
    "openssl"       = "OpenSSL_license.txt"
    "freetype"      = "freetype_FTL.TXT"
    "libjpeg-turbo" = "libjpeg_README.txt"
    "zlib"          = "zlib_README.txt"
}

function Get-DependencyNames {
    <#
        vcpkg.json entries are either a bare string or an object with a "name" (and
        usually a "platform" constraint, e.g. sentry-native on Windows). The previous
        version of this script interpolated the object directly, emitting a .NET type
        name instead of the port name.
    #>
    param($Dependencies)

    $names = @()
    foreach ($dependency in $Dependencies) {
        if ($dependency -is [string]) {
            $names += $dependency
        } elseif ($null -ne $dependency.name) {
            $platform = ""
            if ($null -ne $dependency.PSObject.Properties['platform']) {
                $platform = $dependency.platform
            }
            if ([string]::IsNullOrWhiteSpace($platform)) {
                $names += $dependency.name
            } else {
                $names += "$($dependency.name) [$platform]"
            }
        }
    }
    return $names
}

function Get-InstalledVersion {
    param([string]$Port, [string]$StatusFilePath)

    if ([string]::IsNullOrWhiteSpace($StatusFilePath) -or -not (Test-Path -LiteralPath $StatusFilePath)) {
        return ""
    }

    # vcpkg/status is a Debian-style stanza file: blank-line separated "Key: value" blocks.
    $stanzas = (Get-Content -LiteralPath $StatusFilePath -Raw) -split "(?:\r?\n){2,}"
    foreach ($stanza in $stanzas) {
        if ($stanza -match "(?m)^Package:\s*$([regex]::Escape($Port))\s*$") {
            $version = ""
            $portVersion = ""
            if ($stanza -match "(?m)^Version:\s*(.+?)\s*$") { $version = $Matches[1] }
            if ($stanza -match "(?m)^Port-Version:\s*(.+?)\s*$") { $portVersion = $Matches[1] }
            if ($version -and $portVersion) { return "$version#$portVersion" }
            return $version
        }
    }
    return ""
}

function Get-LicenseText {
    param([string]$Port, [string]$InstalledRoot, [string]$TripletName)

    if (-not [string]::IsNullOrWhiteSpace($InstalledRoot)) {
        $copyright = Join-Path $InstalledRoot "$TripletName\share\$Port\copyright"
        if (Test-Path -LiteralPath $copyright) {
            return @{
                Text   = (Get-Content -LiteralPath $copyright -Raw)
                Source = "vcpkg: $TripletName/share/$Port/copyright"
            }
        }
    }

    if ($fallbackLicenseFiles.ContainsKey($Port)) {
        $fileName = $fallbackLicenseFiles[$Port]
        $fallback = Join-Path $committedLicenseDir $fileName
        if (Test-Path -LiteralPath $fallback) {
            return @{
                Text   = (Get-Content -LiteralPath $fallback -Raw)
                Source = "3rdparty_licenses/$fileName"
            }
        }
    }

    return $null
}

$manifest = Get-Content -LiteralPath $VcpkgJson -Raw | ConvertFrom-Json
$dependencyNames = Get-DependencyNames -Dependencies $manifest.dependencies

if ([string]::IsNullOrWhiteSpace($VcpkgInstalledDir)) {
    Write-Warning "VCPKG_INSTALLED_DIR not set; falling back to committed licenses. Versions will be omitted."
    $VcpkgInstalledDir = ""
} elseif (-not (Test-Path -LiteralPath $VcpkgInstalledDir)) {
    Write-Warning "vcpkg installed dir not found at $VcpkgInstalledDir; falling back to committed licenses."
    $VcpkgInstalledDir = ""
}

$statusFile = ""
if (-not [string]::IsNullOrWhiteSpace($VcpkgInstalledDir)) {
    $statusFile = Join-Path $VcpkgInstalledDir "vcpkg\status"
}

$lines = @(
    "Frisket-PDF Third-Party Notices",
    "Generated: $(Get-Date -Format o)",
    "Source manifest: vcpkg.json ($($manifest.'version-string'))",
    "",
    "This file lists third-party components declared in vcpkg.json, with license text",
    "as resolved at generation time. See docs/PACKAGING_LICENSING.md for the",
    "default-bundle policy (C++/Qt only; no Ghostscript, JRE, or Python).",
    "",
    ("=" * 78),
    "SUMMARY",
    ("=" * 78),
    ""
)

$resolved = @()
$missing = @()

foreach ($name in $dependencyNames) {
    # Strip any "[platform]" annotation to get the bare port name.
    $port = ($name -replace '\s*\[.*\]$', '')
    $version = Get-InstalledVersion -Port $port -StatusFilePath $statusFile
    $license = Get-LicenseText -Port $port -InstalledRoot $VcpkgInstalledDir -TripletName $Triplet

    $versionLabel = if ([string]::IsNullOrWhiteSpace($version)) { "version not resolved" } else { $version }

    if ($null -eq $license) {
        $missing += $port
        $lines += "- $name ($versionLabel) -- LICENSE TEXT NOT FOUND"
    } else {
        $resolved += @{ Name = $name; Version = $versionLabel; License = $license }
        $lines += "- $name ($versionLabel)"
    }
}

$lines += ""
$lines += "Qt 6 is also redistributed with the application. Qt is used under the LGPL v3;"
$lines += "see docs/PACKAGING_LICENSING.md for the relink obligation and the"
$lines += "corresponding-source record."
$lines += ""

foreach ($entry in $resolved) {
    $lines += ("=" * 78)
    $lines += "$($entry.Name) -- $($entry.Version)"
    $lines += "License source: $($entry.License.Source)"
    $lines += ("=" * 78)
    $lines += ""
    $lines += ($entry.License.Text -split "\r?\n")
    $lines += ""
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
Set-Content -LiteralPath $resolvedOutput -Value ($lines -join [Environment]::NewLine) -Encoding UTF8
Write-Host "Wrote $resolvedOutput"
Write-Host "Resolved $($resolved.Count) component(s) with license text."

if ($missing.Count -gt 0) {
    $message = "No license text for: $($missing -join ', '). Add a file to 3rdparty_licenses/ and map it in " +
               "the fallbackLicenseFiles table, or re-run with VCPKG_INSTALLED_DIR pointing at the build's vcpkg tree."
    if ($Strict) {
        throw $message
    }
    Write-Warning $message
}
