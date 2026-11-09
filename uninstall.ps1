<#
    Removes the drop-in DLL from a Family Time installation.

    Usage:
      powershell -ExecutionPolicy Bypass -File uninstall.ps1 -GameDir "C:\Games\Family Time"
#>
param(
    [Parameter(Mandatory = $true)][string]$GameDir
)

$ErrorActionPreference = "Stop"

$target = Join-Path $GameDir "version.dll"

if (-not (Test-Path $target)) {
    Write-Host "Nothing to remove: no version.dll in $GameDir"
    return
}

Remove-Item $target -Force
Write-Host "Removed $target"
Write-Host "The game folder is back to its original contents."
