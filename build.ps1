<#
    Builds the drop-in DLL into dist\.

    Usage:
      powershell -ExecutionPolicy Bypass -File build.ps1
      powershell -ExecutionPolicy Bypass -File build.ps1 -Compiler "C:\msys64\mingw64\bin\gcc.exe"

    Requires a 64-bit C compiler. MinGW-w64 gcc is what this script uses by
    default; MSVC also works, see README.md.
#>
param(
    [string]$Compiler = "gcc",
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "src\version.c"
$out = Join-Path $root $OutDir
$dll = Join-Path $out "version.dll"

if (-not (Test-Path $src)) { throw "Missing source: $src" }

$cc = Get-Command $Compiler -ErrorAction SilentlyContinue
if (-not $cc) { throw "Compiler not found: $Compiler. Install MinGW-w64 or pass -Compiler." }

New-Item -ItemType Directory -Force -Path $out | Out-Null

Write-Host "Building version.dll..."
& $cc.Source -O2 -shared -D_WIN32_WINNT=0x0601 -static-libgcc `
    -o $dll $src -luser32 -lkernel32
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

# The forwarded exports must be present or the player will fail to start.
Write-Host "Exports:"
& (Join-Path $root "tools\list-exports.ps1") -Path $dll

$hash = (Get-FileHash -Algorithm SHA256 -Path $dll).Hash.ToLower()
$sums = Join-Path $out "SHA256SUMS.txt"
Set-Content -Path $sums -Value "$hash  version.dll" -Encoding ASCII -NoNewline
Add-Content -Path $sums -Value "`n" -Encoding ASCII -NoNewline

Write-Host ""
Write-Host "Built:"
Get-ChildItem $out -File | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "$hash  version.dll"
