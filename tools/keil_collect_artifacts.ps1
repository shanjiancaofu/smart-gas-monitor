param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('hw', 'soft')]
    [string]$Mode
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$objects = Join-Path $root "build\keil5\$Mode\Objects"
$artifacts = Join-Path $root "build\keil5\$Mode\Artifacts"
$base = "smart_gas_monitor_keil_$Mode"
$axf = Join-Path $objects "$base.axf"
$hex = Join-Path $objects "$base.hex"
if (-not (Test-Path -LiteralPath $axf)) { throw "AXF file not found: $axf" }
New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
$fromelf = Get-Command fromelf.exe -ErrorAction SilentlyContinue
if (-not $fromelf) { $fromelf = Get-Item 'C:\Keil514\ARM\ARMCC\Bin\fromelf.exe' }
& $fromelf.FullName --bin --output (Join-Path $artifacts "$base.bin") $axf
if ($LASTEXITCODE -ne 0) { throw "fromelf failed: $LASTEXITCODE" }
Move-Item -LiteralPath $axf -Destination (Join-Path $artifacts "$base.axf") -Force
if (Test-Path -LiteralPath $hex) { Move-Item -LiteralPath $hex -Destination (Join-Path $artifacts "$base.hex") -Force }
Get-ChildItem -LiteralPath $objects -Filter "$base*.htm" -File -ErrorAction SilentlyContinue |
    Move-Item -Destination $artifacts -Force
