#Requires -Version 5.1
<#
.SYNOPSIS
    Inject WiX Component entries for DLLs present in the install tree but missing from Product.wxs.

.DESCRIPTION
    Product.wxs.in is a hand-maintained allowlist. cmake --install can stage additional
    runtime DLLs (ICU, Qt transitive deps, vcpkg libs). Light only packages files listed
    in Product.wxs, so MSI installs can miss load-time deps that the staged tree has.
    This script closes that gap for *.dll under InstallBinDir (not subdirs).
#>
param(
    [Parameter(Mandatory = $true)][string]$ProductWxs,
    [Parameter(Mandatory = $true)][string]$InstallBinDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $ProductWxs)) {
    throw "Product.wxs not found: $ProductWxs"
}
if (-not (Test-Path -LiteralPath $InstallBinDir)) {
    throw "Install bin dir not found: $InstallBinDir"
}

$wxs = Get-Content -LiteralPath $ProductWxs -Raw
$dlls = @(Get-ChildItem -LiteralPath $InstallBinDir -Filter "*.dll" -File)
$missing = @()
foreach ($dll in $dlls) {
    if ($wxs -notmatch [regex]::Escape($dll.Name)) {
        $missing += $dll
    }
}

if ($missing.Count -eq 0) {
    Write-Host "OK: all $($dlls.Count) top-level install DLLs are referenced in Product.wxs"
    exit 0
}

Write-Host "Injecting $($missing.Count) DLL(s) missing from Product.wxs:"
$sb = New-Object System.Text.StringBuilder
$index = 0
foreach ($dll in $missing) {
    $index++
    $safe = ($dll.BaseName -replace '[^A-Za-z0-9]', '')
    if ([string]::IsNullOrWhiteSpace($safe)) { $safe = "dll$index" }
    $guid = [guid]::NewGuid().ToString().ToUpperInvariant()
    Write-Host "  + $($dll.Name)"
    [void]$sb.AppendLine("      <Component Id=`"cmpAuto$safe$index`" Directory=`"INSTALLFOLDER`" Guid=`"{$guid}`">")
    [void]$sb.AppendLine("        <File Id=`"filAuto$safe$index`" KeyPath=`"yes`" Source=`"`$(var.MyInstallDir)\$($dll.Name)`" />")
    [void]$sb.AppendLine("      </Component>")
}

$marker = '<Component Id="cmpQt6Core"'
if ($wxs -notmatch [regex]::Escape($marker)) {
    throw "Could not find insertion marker cmpQt6Core in $ProductWxs"
}
$injection = $sb.ToString() + "      " + $marker
$updated = $wxs.Replace($marker, $injection.TrimStart())
# PowerShell Replace is fine; marker appears once.
Set-Content -LiteralPath $ProductWxs -Value $updated -NoNewline
Write-Host "Updated $ProductWxs"
