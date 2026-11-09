param([Parameter(Mandatory = $true)][string]$Path)

# Reads the PE export table directly, so no compiler or dumpbin is required at
# runtime beyond the build that produced the DLL.
$bytes = [System.IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)

$e_lfanew = [BitConverter]::ToInt32($bytes, 0x3C)
$optional = $e_lfanew + 24
$magic = [BitConverter]::ToUInt16($bytes, $optional)

# PE32+ (0x20B) export directory is at optional header offset 0x70; PE32 at 0x60.
$dirOffset = if ($magic -eq 0x20B) { 0x70 } else { 0x60 }
$exportRva = [BitConverter]::ToUInt32($bytes, $optional + $dirOffset)

$sectionStart = $optional + [BitConverter]::ToUInt16($bytes, $e_lfanew + 20)
$sectionCount = [BitConverter]::ToUInt16($bytes, $e_lfanew + 6)

function Convert-RvaToOffset([uint32]$rva) {
    for ($i = 0; $i -lt $sectionCount; $i++) {
        $s = $sectionStart + ($i * 40)
        $virtSize = [BitConverter]::ToUInt32($bytes, $s + 8)
        $virtAddr = [BitConverter]::ToUInt32($bytes, $s + 12)
        $rawSize = [BitConverter]::ToUInt32($bytes, $s + 16)
        $rawAddr = [BitConverter]::ToUInt32($bytes, $s + 20)
        $span = [Math]::Max($virtSize, $rawSize)
        if ($rva -ge $virtAddr -and $rva -lt ($virtAddr + $span)) {
            return [uint32]($rva - $virtAddr + $rawAddr)
        }
    }
    return 0
}

if ($exportRva -eq 0) {
    Write-Host "  (no export directory)"
    return
}

$exportOff = Convert-RvaToOffset $exportRva
$nameCount = [BitConverter]::ToInt32($bytes, $exportOff + 24)
$namesRva = [BitConverter]::ToUInt32($bytes, $exportOff + 32)
$namesOff = Convert-RvaToOffset $namesRva

for ($i = 0; $i -lt $nameCount; $i++) {
    $nameRva = [BitConverter]::ToUInt32($bytes, $namesOff + ($i * 4))
    $off = Convert-RvaToOffset $nameRva
    $end = $off
    while ($bytes[$end] -ne 0) { $end++ }
    $name = [System.Text.Encoding]::ASCII.GetString($bytes, $off, $end - $off)
    Write-Host "  $name"
}

Write-Host "  ($nameCount exports)"
