<#
    Copies version.dll into a Family Time installation.

    Usage:
      powershell -ExecutionPolicy Bypass -File install.ps1 -GameDir "C:\Games\Family Time"

    You can also just copy dist\version.dll into the folder that contains
    FamilyTime.exe. This script only validates the folder and reports what it did.
#>
param(
    [Parameter(Mandatory = $true)][string]$GameDir
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll = Join-Path $root "dist\version.dll"

if (-not (Test-Path $dll)) { throw "Missing $dll. Run build.ps1 first, or use a release download." }
if (-not (Test-Path (Join-Path $GameDir "FamilyTime.exe"))) {
    throw "No FamilyTime.exe in '$GameDir'. Point -GameDir at the folder that contains the game exe."
}
if (Test-Path (Join-Path $GameDir "version.dll")) {
    throw "version.dll already exists in '$GameDir'. Remove it first (uninstall.ps1) so nothing unexpected is overwritten."
}

Copy-Item $dll (Join-Path $GameDir "version.dll") -Force
Write-Host "Installed version.dll into $GameDir"
Write-Host "Start the game normally. The mod writes a log to %LOCALAPPDATA%\FamilyTimeNoPause\log.txt"
Write-Host "To remove it, run: powershell -File uninstall.ps1 -GameDir `"$GameDir`""
